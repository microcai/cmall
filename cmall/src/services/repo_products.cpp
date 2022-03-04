
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/repo_products.hpp"
#include "utils/logging.hpp"

#include "gitpp/gitpp.hpp"

#include "./comma_kv_grammer.hpp"
#include "cmall/error_code.hpp"

namespace services
{
	struct repo_products_impl
	{
		repo_products_impl(boost::asio::io_context& io, boost::filesystem::path repo_path)
			: io(io)
			, repo_path(repo_path)
			, git_repo(repo_path)
		{
			gitpp::reference git_head = git_repo.head();

			// auto tree = git_repo.get_tree(git_head.target());
		}

		template<typename WalkHandler>
		void treewalk(const gitpp::tree& tree, boost::filesystem::path parent_dir_in_git, WalkHandler walker)
		{
			for (const gitpp::tree_entry & tree_entry : tree)
			{
				switch (tree_entry.type())
				{
					case GIT_OBJECT_TREE:
						treewalk(git_repo.get_tree_by_treeid(tree_entry.get_oid()), parent_dir_in_git / tree_entry.name(), walker);
						break;
					case GIT_OBJECT_BLOB:
					{
						if (walker(tree_entry, parent_dir_in_git))
							break;
					}
					break;
					default:
					break;
		 		}
			}
		}

		static product to_product(std::string_view markdown_content, boost::system::error_code& ec)
		{
			// 寻找 --- ---
			auto first_pos = markdown_content.find("---\n");
			if (first_pos >= 0)
			{
				auto second_pos = markdown_content.find("---\n", first_pos + 4);

				if (second_pos > first_pos + 4)
				{
					std::string md_str(markdown_content.begin() + first_pos, markdown_content.begin() + second_pos + 4);
					auto result = parse_comma_kv(md_str);
					if (result)
					{
						product founded;
						founded.product_title = result->title;
						founded.product_price = result->price;
						founded.product_description = result->description;
						founded.pics.push_back(result->picture);
						founded.detailed = markdown_content.substr(second_pos + 4);
						return founded;
					}
				}
			}
			ec = boost::asio::error::not_found;
			return product{};
		}

		std::string get_file_content(boost::filesystem::path look_for_file, boost::system::error_code& ec)
		{
			std::string ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), boost::filesystem::path(""), [&look_for_file, &ret, this](const gitpp::tree_entry & tree_entry, boost::filesystem::path parent_path) mutable
				{
					boost::filesystem::path entry_filename(parent_path / tree_entry.name());

					if (entry_filename == look_for_file)
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						ret = file_blob.get_content();
						return true;
					}
					return false;
				});

			return ret;
		}

		product get_products(std::string goods_id, boost::system::error_code& ec)
		{
			std::vector<product> ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), boost::filesystem::path(""), [goods_id, &commit_version, &ret, &ec, this](const gitpp::tree_entry & tree_entry, boost::filesystem::path) mutable
				{
					boost::filesystem::path entry_filename(tree_entry.name());

					if (entry_filename.stem().string() == goods_id && entry_filename.has_extension() && entry_filename.extension() == ".md")
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						std::string_view md = file_blob.get_content();

						product to_be_append = to_product(md, ec);
						if (ec)
							return false;
						to_be_append.git_version = commit_version.as_sha1_string();
						to_be_append.product_id = entry_filename.stem().string();

						ret.push_back(to_be_append);
						return true;
					}
					return false;
				});

			if (ret.size() > 0)
				return ret[0];
			ec = cmall::error::goods_not_found;
			return product{};
		}

		std::vector<product> get_products(boost::system::error_code& ec)
		{
			ec = cmall::error::goods_not_found;
			std::vector<product> ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), boost::filesystem::path(""), [&, this](const gitpp::tree_entry & tree_entry, boost::filesystem::path parent_dir_in_git)
				{
					boost::filesystem::path entry_filename(tree_entry.name());
					if (entry_filename.has_extension() && entry_filename.extension() == ".md")
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						std::string_view md = file_blob.get_content();

						product to_be_append = to_product(md, ec);
						if (!ec)
						{
							to_be_append.git_version = commit_version.as_sha1_string();
							to_be_append.product_id = entry_filename.stem().string();
							ec = boost::system::error_code{};
							ret.push_back(to_be_append);
						}
					}
					return false;
				});

			return ret;
		}

		boost::asio::io_context& io;
		boost::filesystem::path repo_path;
		gitpp::repo git_repo;
	};

	repo_products::repo_products(boost::asio::io_context& io, boost::filesystem::path repo_path)
	{
		static_assert(sizeof(obj_stor) >= sizeof(repo_products_impl));
		std::construct_at(reinterpret_cast<repo_products_impl*>(obj_stor.data()), io, repo_path);
	}

	boost::asio::awaitable<std::string> repo_products::get_file_content(boost::filesystem::path path, boost::system::error_code& ec)
	{
		return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
			void(boost::system::error_code, std::string)>(
			[this, path, &ec](auto&& handler) mutable
			{
				boost::asio::post(impl().io,
					[this, &ec, path, handler = std::move(handler)]() mutable
					{
						auto ret = impl().get_file_content(path, ec);
						auto excutor = boost::asio::get_associated_executor(handler, impl().io);
						boost::asio::post(excutor, [handler = std::move(handler), ret]() mutable { handler(boost::system::error_code(), ret);});
					});
			},
			boost::asio::use_awaitable);
	}

	// 从给定的 goods_id 找到商品定义.
	boost::asio::awaitable<product> repo_products::get_products(std::string goods_id)
	{
		return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
			void(boost::system::error_code, product)>(
			[this, goods_id](auto&& handler) mutable
			{
				boost::asio::post(impl().io,
					[this, goods_id, handler = std::move(handler)]() mutable
					{
						boost::system::error_code ec;
						auto ret = impl().get_products(goods_id, ec);
						auto excutor = boost::asio::get_associated_executor(handler, impl().io);
						boost::asio::post(excutor, [handler = std::move(handler), ret, ec]() mutable { handler(ec, ret);});
					});
			},
			boost::asio::use_awaitable);
	}

	boost::asio::awaitable<std::vector<product>> repo_products::get_products()
	{
		return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
			void(boost::system::error_code, std::vector<product>)>(
			[this](auto&& handler) mutable
			{
				boost::asio::post(impl().io,
					[this, handler = std::move(handler)]() mutable
					{
						boost::system::error_code ec;
						auto ret = impl().get_products(ec);
						auto excutor = boost::asio::get_associated_executor(handler, impl().io);
						boost::asio::post(excutor, [handler = std::move(handler), ret, ec]() mutable { handler(ec, ret);});
					});
			},
			boost::asio::use_awaitable);
	}


	repo_products::~repo_products()
	{
		std::destroy_at(reinterpret_cast<repo_products_impl*>(obj_stor.data()));
	}

	const repo_products_impl& repo_products::impl() const
	{
		return *reinterpret_cast<const repo_products_impl*>(obj_stor.data());
	}

	repo_products_impl& repo_products::impl()
	{
		return *reinterpret_cast<repo_products_impl*>(obj_stor.data());
	}

	bool repo_products::init_bare_repo(boost::filesystem::path repo_path)
	{
		return gitpp::init_bare_repo(repo_path);
	}

	bool repo_products::is_git_repo(boost::filesystem::path repo_path)
	{
		return gitpp::is_git_repo(repo_path);
	}

}
