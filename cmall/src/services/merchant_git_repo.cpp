
#include "stdafx.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/promise.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string/split.hpp>

#include <boost/throw_exception.hpp>
#include "services/merchant_git_repo.hpp"
#include "utils/logging.hpp"

#include "gitpp/gitpp.hpp"

#include "goods_md_metadata_grammer.hpp"
#include "md_transpile.hpp"
#include "md_render.hpp"
#include "cmall/error_code.hpp"
#include "utils/coroyield.hpp"

using boost::asio::use_awaitable;
using boost::asio::experimental::use_promise;

namespace services
{
	struct merchant_git_repo_impl
	{
		merchant_git_repo_impl(std::uint64_t merchant_id, std::filesystem::path repo_path_)
			: repo_path_(repo_path_)
			, git_repo(repo_path_)
			, merchant_id(merchant_id)
		{
		}

		template<typename WalkHandler>
		void treewalk(const gitpp::tree& tree, std::filesystem::path parent_dir_in_git, WalkHandler walker)
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

		std::string correct_url(std::string original_url, std::string baseurl)
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
				LOG_DBG << std::format("replace_url ({}) as ({})", original_url, (baseurl + std::to_string(merchant_id) + "/images/" + w[5].str()));
				// 只要找到了 ../images/... 这样的路径, 就替换为 /repos/
				return baseurl + "/repos/" + std::to_string(merchant_id) + "/images/" + w[5].str();
			}
			else if (boost::regex_match(original_url, w, boost::regex(R"raw(((\/)|(\.\/)|(\.\.\/))?css\/(.*))raw")))
			{
				LOG_DBG << std::format("replace_url ({}) as ({})", original_url, (baseurl + std::to_string(merchant_id) + "/css/" + w[5].str()));
				// 只要找到了 ../images/... 这样的路径, 就替换为 /repos/
				return baseurl + "/repos/" + std::to_string(merchant_id) + "/css/" + w[5].str();
			}
			return original_url;
		}

		std::tuple<std::string_view, std::string_view> split_markdown(std::string_view md, boost::system::error_code& ec)
		{
			// 寻找 --- ---
			auto first_pos = md.find("---\n");
			if (first_pos != std::string::npos)
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

		product to_product(std::string_view markdown_content, std::string_view merchant_name, std::string baseurl, boost::system::error_code& ec)
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
					founded.pics.push_back(correct_url(pic_url, baseurl));
				founded.merchant_id = merchant_id;
				founded.merchant_name = merchant_name;
				for (auto & keywording : result->keywords)
					founded.keywords.push_back({.keyword = keywording, .rank = 1.0});
				return founded;
			}

			ec = boost::asio::error::not_found;
			return product{};
		}

		std::string get_file_content(std::filesystem::path look_for_file, boost::system::error_code& ec)
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

		product get_product(std::string goods_id, std::string_view merchant_name, std::string baseurl, boost::system::error_code& ec)
		{
			std::vector<product> ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), std::filesystem::path(""), [baseurl, merchant_name, goods_id, &commit_version, &ret, &ec, this](const gitpp::tree_entry & tree_entry, std::filesystem::path) mutable
				{
					std::filesystem::path entry_filename(tree_entry.name());

					if (entry_filename.stem().string() == goods_id && entry_filename.has_extension() && entry_filename.extension() == ".md")
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						std::string_view md = file_blob.get_content();

						product to_be_append = to_product(md, merchant_name, baseurl, ec);
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

		int get_products(std::vector<product>& ret, std::string merchant_name, std::string baseurl)
		{
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), std::filesystem::path(""), [&, baseurl, this](const gitpp::tree_entry & tree_entry, std::filesystem::path parent_dir_in_git)
				{
					std::filesystem::path entry_filename(tree_entry.name());
					if (entry_filename.has_extension() && entry_filename.extension() == ".md")
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						std::string_view md = file_blob.get_content();

						boost::system::error_code ec;
						product to_be_append = to_product(md, merchant_name, baseurl, ec);
						if (!ec)
						{
							to_be_append.git_version = commit_version.as_sha1_string();
							to_be_append.product_id = entry_filename.stem().string();
							ret.push_back(to_be_append);
						}
					}
					return false;
				});

			return (int)ret.size();
		}

		std::string get_transpiled_md(std::filesystem::path md_file_path, std::string baseurl, boost::system::error_code& ec)
		{
			std::string ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto look_for_file_entry = repo_tree.by_path(md_file_path.string());

			ec = boost::asio::error::not_found;

			if (!look_for_file_entry.empty())
			{
				auto file_blob = git_repo.get_blob(look_for_file_entry.get_oid());
				ret = md::markdown_transpile(file_blob.get_content(), std::bind(&merchant_git_repo_impl::correct_url, this, std::placeholders::_1, baseurl));

				ec = boost::system::error_code{};
				return ret;
			}

			return ret;
		}

		std::string get_product_detail(std::string goods_id, std::string baseurl, boost::system::error_code& ec)
		{
			ec = cmall::error::goods_not_found;
			std::string ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), std::filesystem::path(""), [baseurl, goods_id, &ret, &ec, this](const gitpp::tree_entry & tree_entry, std::filesystem::path) mutable -> bool
				{
					std::filesystem::path entry_filename(tree_entry.name());

					if (entry_filename.stem().string() == goods_id && entry_filename.has_extension() && entry_filename.extension() == ".md")
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						std::string_view md = file_blob.get_content();

						auto [meta, body] = split_markdown(md, ec);

						if (ec)
							return false;

						ret = md::markdown_transpile(body, std::bind(&merchant_git_repo_impl::correct_url, this, std::placeholders::_1, baseurl));
						return true;
					}
					return false;
				});

			return ret;
		}

		std::string get_product_html(std::string goods_id, std::string baseurl, boost::system::error_code& ec)
		{
			ec = cmall::error::goods_not_found;
			std::string ret;
			gitpp::oid commit_version = git_repo.head().target();
			gitpp::tree repo_tree = git_repo.get_tree_by_commit(commit_version);

			auto goods = repo_tree.by_path("goods");

			if (!goods.empty())
				treewalk(git_repo.get_tree_by_treeid(goods.get_oid()), std::filesystem::path(""), [baseurl, goods_id, &ret, &ec, this](const gitpp::tree_entry & tree_entry, std::filesystem::path) mutable -> bool
				{
					std::filesystem::path entry_filename(tree_entry.name());

					if (entry_filename.stem().string() == goods_id && entry_filename.has_extension() && entry_filename.extension() == ".md")
					{
						auto file_blob = git_repo.get_blob(tree_entry.get_oid());
						std::string_view md = file_blob.get_content();

						auto [meta, body] = split_markdown(md, ec);

						if (ec)
							return false;

						ret = mdrender::markdown_to_html(body, std::bind(&merchant_git_repo_impl::correct_url, this, std::placeholders::_1, baseurl));
						return true;
					}
					return false;
				});

			return ret;
		}

		bool check_repo_changed(std::string old_head) const
		{
			return git_repo.head().target().as_sha1_string() != old_head;
		}

		std::string git_head() const
		{
			return git_repo.head().target().as_sha1_string();
		}

		std::vector<std::string> get_supported_payment()
		{
			boost::system::error_code ec;

			auto content = get_file_content("settings.ini", ec);
			if (!ec)
			{
				try
				{
					namespace pt = boost::property_tree;
					pt::ptree ptree;
					std::istringstream is(content);
					pt::ini_parser::read_ini(is, ptree);
					auto methods = ptree.get<std::string>("Payment.methods"); // alipay;unionpay;rncoldwallet

					std::vector<std::string> result;
					boost::split(result, methods, boost::is_any_of(",，"));

					return result;
				}
				catch (std::exception& e)
				{
					LOG_ERR << "error parsing supported payment: " << e.what();
				}

			}

			return std::vector<std::string>{};
		}

		std::filesystem::path repo_path() const
		{
			return repo_path_;
		}

		std::filesystem::path repo_path_;
		gitpp::repo git_repo;
		std::uint64_t merchant_id;
	};

	awaitable<std::string> merchant_git_repo::get_file_content(std::filesystem::path path, boost::system::error_code& ec)
	{
		std::string ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			ret = impl().get_file_content(path, ec);
		}, use_awaitable);
		co_return ret;
	}

	// 从给定的 goods_id 找到商品定义.
	awaitable<product> merchant_git_repo::get_product(std::string goods_id, std::string_view merchant_name, std::string baseurl)
	{
		product ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			boost::system::error_code ec;
			co_await this_coro::coro_yield();
			ret = impl().get_product(goods_id, merchant_name, baseurl, ec);
			if (ec)
				boost::throw_exception(boost::system::system_error(ec));
			co_return;
		}, use_awaitable);
		co_return ret;
	}

	// 从给定的 goods_id 找到商品定义.
	awaitable<product> merchant_git_repo::get_product(std::string goods_id, std::string_view merchant_name, std::string baseurl, boost::system::error_code& ec)
	{
		product ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			ret = impl().get_product(goods_id, merchant_name, baseurl, ec);
		}, use_awaitable);
		co_return ret;
	}

	awaitable<std::vector<product>> merchant_git_repo::get_products(std::string merchant_name, std::string baseurl)
	{
		std::vector<product> ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<int> {
			co_await this_coro::coro_yield();
			co_return impl().get_products(ret, merchant_name, baseurl);
		}, use_awaitable);
		co_return ret;
	}

	awaitable<std::string> merchant_git_repo::get_transpiled_md(std::string md_file_path, std::string baseurl)
	{
		std::string ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			boost::system::error_code ec;
			co_await this_coro::coro_yield();
			ret = impl().get_transpiled_md(md_file_path, baseurl, ec);
			if (ec)
				boost::throw_exception(boost::system::system_error(ec));
			co_return;
		}, use_awaitable);
		co_return ret;
	}

	awaitable<std::string> merchant_git_repo::get_transpiled_md(std::string md_file_path, std::string baseurl, boost::system::error_code& ec)
	{
		std::string ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			ret = impl().get_transpiled_md(md_file_path, baseurl, ec);
			co_return;
		}, use_awaitable);
		co_return ret;
	}

	awaitable<std::string> merchant_git_repo::get_product_detail(std::string goods_id, std::string baseurl)
	{
		std::string ret;

		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			boost::system::error_code ec;
			ret = impl().get_product_detail(goods_id, baseurl, ec);
			if (ec)
				boost::throw_exception(boost::system::system_error(ec));
			co_return;
		}, use_awaitable);
		co_return ret;
	}

	awaitable<std::string> merchant_git_repo::get_product_detail(std::string goods_id, std::string baseurl, boost::system::error_code& ec)
	{
		std::string ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			ret = impl().get_product_detail(goods_id, baseurl, ec);
			co_return;
		}, use_awaitable);
		co_return ret;
	}

	awaitable<std::string> merchant_git_repo::get_product_html(std::string goods_id, std::string baseurl)
	{
		std::string ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			boost::system::error_code ec;
			ret = impl().get_product_html(goods_id, baseurl, ec);
			if (ec)
				boost::throw_exception(boost::system::system_error(ec));
			co_return;
		}, use_awaitable);
		co_return ret;
	}


	awaitable<std::vector<std::string>> merchant_git_repo::get_supported_payment()
	{
		std::vector<std::string> supported_payment;
		co_await boost::asio::co_spawn(thread_pool, [&supported_payment, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			supported_payment = impl().get_supported_payment();
			co_return;
		}, use_awaitable);
		co_return supported_payment;
	}

	std::uint64_t merchant_git_repo::get_merchant_uid() const
	{
		return impl().merchant_id;
	}

	awaitable<bool> merchant_git_repo::check_repo_changed(std::string old_head) const
	{
		co_return co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<bool> {
			co_await this_coro::coro_yield();
			co_return impl().check_repo_changed(old_head);
		}, use_awaitable);
	}

	awaitable<std::string> merchant_git_repo::git_head() const
	{
		std::string ret;
		co_await boost::asio::co_spawn(thread_pool, [&, this]() mutable -> awaitable<void> {
			co_await this_coro::coro_yield();
			ret = impl().git_head();
		}, use_awaitable);
		co_return ret;
	}

	std::filesystem::path merchant_git_repo::repo_path() const
	{
		return impl().repo_path();
	}

	merchant_git_repo::merchant_git_repo(boost::asio::thread_pool& executor, std::uint64_t merchant_id, std::filesystem::path repo_path)
		: thread_pool(executor)
	{
		static_assert(sizeof(obj_stor) >= sizeof(merchant_git_repo_impl));
		std::construct_at(reinterpret_cast<merchant_git_repo_impl*>(obj_stor.data()), merchant_id, repo_path);
	}

	merchant_git_repo::~merchant_git_repo()
	{
		std::destroy_at(reinterpret_cast<merchant_git_repo_impl*>(obj_stor.data()));
	}

	const merchant_git_repo_impl& merchant_git_repo::impl() const
	{
		return *reinterpret_cast<const merchant_git_repo_impl*>(obj_stor.data());
	}

	merchant_git_repo_impl& merchant_git_repo::impl()
	{
		return *reinterpret_cast<merchant_git_repo_impl*>(obj_stor.data());
	}

	bool merchant_git_repo::init_bare_repo(std::filesystem::path repo_path)
	{
		return gitpp::init_bare_repo(repo_path);
	}

	bool merchant_git_repo::is_git_repo(std::filesystem::path repo_path)
	{
		return gitpp::is_git_repo(repo_path);
	}

}
