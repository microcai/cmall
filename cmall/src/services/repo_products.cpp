﻿
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/repo_products.hpp"
#include "utils/logging.hpp"

#include "gitpp/gitpp.hpp"

#include "./comma_kv_grammer.hpp"

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

		void extrace_product(const gitpp::tree& tree, std::vector<product>& appendee)
		{
			for (const gitpp::tree_entry & tree_entry : tree)
			{
				switch (tree_entry.type())
				{
					case GIT_OBJECT_TREE:
					std::cerr <<"recursive git repo:" << tree_entry.name() << std::endl;
						extrace_product(git_repo.get_tree_by_treeid(tree_entry.get_oid()), appendee);
					break;
					case GIT_OBJECT_BLOB:
					{
						std::cerr <<"traval git repo:" << tree_entry.name()<< std::endl;
						boost::filesystem::path entry_filename(tree_entry.name());

						if (entry_filename.has_extension() && entry_filename.extension() == ".md")
						{
							auto file_blob = git_repo.get_blob(tree_entry.get_oid());
							std::string_view md = file_blob.get_content();

							// 寻找 --- --- 
							auto first_pos = md.find_first_of("---");
							if (first_pos >= 0)
							{
								auto second_pos = md.find_first_of("---", first_pos + 4);

								if (second_pos > first_pos + 4)
								{
									std::string md_str(md.begin() + first_pos, md.begin() + second_pos + 3);
									goods_description result = parse_comma_kv(md_str).value();

									product to_be_append;
									to_be_append.product_id = entry_filename.stem().string();
									to_be_append.product_title = result.title;
									to_be_append.product_price = result.price;
									to_be_append.product_description = result.description;
									to_be_append.pics.push_back(result.picture);
									to_be_append.detailed = md.substr(second_pos + 4);

									appendee.push_back(to_be_append);
								}
							}
						}
					}
					break;
					default:
					break;
		 		}

			}

		}

		std::vector<product> get_products(boost::system::error_code&)
		{
			std::vector<product> ret;
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(git_repo.head().target());

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				extrace_product(git_repo.get_tree_by_treeid(goods.get_oid()), ret);

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
						handler(ec, ret);
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
