
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/execution_context.hpp"
#include "boost/asio/thread_pool.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "boost/regex/v5/regex.hpp"
#include "boost/regex/v5/regex_match.hpp"
#include "boost/system/detail/error_code.hpp"
#include "boost/system/system_error.hpp"
#include "boost/throw_exception.hpp"
#include "services/repo_products.hpp"
#include "utils/logging.hpp"

#include "gitpp/gitpp.hpp"

#include "./goods_md_metadata_grammer.hpp"
#include "./md_transpile.hpp"
#include "cmall/error_code.hpp"

namespace services
{
	struct repo_products_impl
	{
		repo_products_impl(std::uint64_t merchant_id, boost::filesystem::path repo_path)
			: repo_path(repo_path)
			, git_repo(repo_path)
			, merchant_id(merchant_id)
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

		std::string correct_url(std::string original_url)
		{
			if (original_url.empty())
				return original_url;

			if (original_url.starts_with("http"))
			{
				return original_url;
			}

			boost::smatch w;
			LOG_DBG << "replace url(" << original_url << ")";
			if (boost::regex_match(original_url, w, boost::regex(R"raw(((\/)|(\.\/)|(\.\.\/))?images\/(.*))raw")))
			{
				LOG_DBG << "replace url(" << original_url << ") as (" << ("/repos/" + std::to_string(merchant_id) + "/images/" + w[5].str());
				// 只要找到了 ../images/... 这样的路径, 就替换为 /repos/
				return "/repos/" + std::to_string(merchant_id) + "/images/" + w[5].str();
			}
			return original_url;
		}

		std::tuple<std::string_view, std::string_view> split_markdown(std::string_view md, boost::system::error_code& ec)
		{
			// 寻找 --- ---
			auto first_pos = md.find("---\n");
			if (first_pos >= 0)
			{
				auto second_pos = md.find("---\n", first_pos + 4);

				if (second_pos > first_pos + 4){
					std::string_view metadata = md.substr(first_pos, second_pos - first_pos + 4);
					std::string_view main_body = md.substr(second_pos + 4);
					ec = boost::system::error_code();
					return std::make_tuple(metadata, main_body);
				}
			}

			ec = boost::asio::error::not_found;
			return std::make_tuple(std::string_view(), std::string_view());
		}

		product to_product(std::string_view markdown_content, boost::system::error_code& ec)
		{
			auto [meta, body] = split_markdown(markdown_content, ec);

			if (ec)
				return product{};

			auto result = parse_goods_metadata(meta);

			if (result)
			{
				product founded;
				founded.product_title = result->title;
				founded.product_price = result->price;
				founded.product_description = result->description;
				for (auto pic_url : result->picture)
					founded.pics.push_back(correct_url(pic_url));
				founded.merchant_id = merchant_id;
				return founded;
			}

			ec = boost::asio::error::not_found;
			return product{};
		}

		std::string get_file_content(boost::filesystem::path look_for_file, boost::system::error_code& ec)
		{
			std::string ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto look_for_file_entry = repo_tree.by_path(look_for_file.string());

			ec = boost::asio::error::not_found;

			if (!look_for_file_entry.empty())
			{
				auto file_blob = git_repo.get_blob(look_for_file_entry.get_oid());
				ret = file_blob.get_content();
				ec = boost::system::error_code{};
				return ret;
			}

			return ret;
		}

		product get_product(std::string goods_id, boost::system::error_code& ec)
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

		std::string get_product_detail(std::string goods_id, boost::system::error_code& ec)
		{
			ec = cmall::error::goods_not_found;
			std::string ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), boost::filesystem::path(""), [goods_id, &ret, &ec, this](const gitpp::tree_entry & tree_entry, boost::filesystem::path) mutable -> bool
				{
					boost::filesystem::path entry_filename(tree_entry.name());

					if (entry_filename.stem().string() == goods_id && entry_filename.has_extension() && entry_filename.extension() == ".md")
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						std::string_view md = file_blob.get_content();

						auto [meta, body] = split_markdown(md, ec);

						if (ec)
							return false;

						ret = md::markdown_transpile(body, std::bind(&repo_products_impl::correct_url, this, std::placeholders::_1));
						return true;
					}
					return false;
				});

			return ret;
		}

		boost::filesystem::path repo_path;
		gitpp::repo git_repo;
		std::uint64_t merchant_id;
	};

	boost::asio::awaitable<std::string> repo_products::get_file_content(boost::filesystem::path path, boost::system::error_code& ec)
	{
		return boost::asio::co_spawn(thread_pool, [path, &ec, this]() mutable -> boost::asio::awaitable<std::string> {
			co_return impl().get_file_content(path, ec);
		}, boost::asio::use_awaitable);
	}

	// 从给定的 goods_id 找到商品定义.
	boost::asio::awaitable<product> repo_products::get_product(std::string goods_id)
	{
		return boost::asio::co_spawn(thread_pool, [goods_id, this]() mutable -> boost::asio::awaitable<product> {
			boost::system::error_code ec;
			auto ret = impl().get_product(goods_id, ec);
			if (ec)
				boost::throw_exception(boost::system::system_error(ec));
			co_return ret;
		}, boost::asio::use_awaitable);
	}

	// 从给定的 goods_id 找到商品定义.
	boost::asio::awaitable<product> repo_products::get_product(std::string goods_id, boost::system::error_code& ec)
	{
		return boost::asio::co_spawn(thread_pool, [goods_id, &ec, this]() mutable -> boost::asio::awaitable<product> {
			co_return impl().get_product(goods_id, ec);
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<std::vector<product>> repo_products::get_products()
	{
		return boost::asio::co_spawn(thread_pool, [this]() mutable -> boost::asio::awaitable<std::vector<product>> {
			boost::system::error_code ec;
			auto ret = impl().get_products(ec);
			if (ec)
				boost::throw_exception(boost::system::system_error(ec));
			co_return ret;
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<std::string> repo_products::get_product_detail(std::string goods_id)
	{
		return boost::asio::co_spawn(thread_pool, [goods_id, this]() mutable -> boost::asio::awaitable<std::string> {
			boost::system::error_code ec;
			auto ret = impl().get_product_detail(goods_id, ec);
			if (ec)
				boost::throw_exception(boost::system::system_error(ec));
			co_return ret;
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<std::string> repo_products::get_product_detail(std::string goods_id, boost::system::error_code& ec)
	{
		return boost::asio::co_spawn(thread_pool, [goods_id, &ec, this]() mutable -> boost::asio::awaitable<std::string> {
			co_return impl().get_product_detail(goods_id, ec);
		}, boost::asio::use_awaitable);
	}

	repo_products::repo_products(boost::asio::thread_pool& executor, std::uint64_t merchant_id, boost::filesystem::path repo_path)
		: thread_pool(executor)
	{
		static_assert(sizeof(obj_stor) >= sizeof(repo_products_impl));
		std::construct_at(reinterpret_cast<repo_products_impl*>(obj_stor.data()), merchant_id, repo_path);
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
