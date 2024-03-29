
#pragma once

#include <filesystem>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system/detail/error_code.hpp>
#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

using boost::asio::awaitable;
using cpp_numeric = boost::multiprecision::cpp_dec_float_100;

namespace services
{
	struct product_keyword
	{
		std::string keyword;
		double rank;
	};

	struct product_option_select
	{
		std::string selection_name;
		cpp_numeric selection_price;
	};

	struct product_option
	{
		std::string option_title;
		std::vector<product_option_select> selections;
	};

	struct product
	{
		// 商品 ID, 实质上是商品描述文件的文件名.
		std::string product_id;
		// 产品缩略图. 其实是 pic: 字段, 可以多个重复.
		std::vector<std::string> pics;

		// 产品标题.
		std::string product_title;
		// 产品简述.
		std::string product_description;

		std::string product_price;

		std::string kuaidifei;

		std::string git_version;

		std::uint64_t merchant_id;
		std::string merchant_name;

		std::vector<product_keyword> keywords;

		std::vector<product_option> options;
	};

	struct merchant_git_repo_impl;
	class merchant_git_repo
	{
	public:

		static bool init_bare_repo(std::filesystem::path repo_path);

		static bool is_git_repo(std::filesystem::path repo_path);

		merchant_git_repo(boost::asio::thread_pool& executor, std::uint64_t merchant_id, std::filesystem::path repo_path);
		~merchant_git_repo();

		awaitable<std::string> get_file_content(std::filesystem::path path, boost::system::error_code& ec);

		// 扫描用户仓库, 返回找到的商品定义.
		awaitable<std::vector<product>> get_products(std::string merchant_name, std::string baseurl);

		// 从给定的 goods_id 找到商品定义.
		awaitable<product> get_product(std::string goods_id, std::string_view merchant_name, std::string baseurl, boost::system::error_code& ec);
		awaitable<product> get_product(std::string goods_id, std::string_view merchant_name, std::string baseurl);

		awaitable<std::string> get_transpiled_md(std::string md_file_path, std::string baseurl);
		awaitable<std::string> get_transpiled_md(std::string md_file_path, std::string baseurl, boost::system::error_code& ec);
		awaitable<std::string> get_product_detail(std::string goods_id, std::string baseurl);
		awaitable<std::string> get_product_detail(std::string goods_id, std::string baseurl, boost::system::error_code& ec);

		awaitable<std::string> get_product_html(std::string goods_id, std::string baseurl);

		awaitable<std::vector<std::string>> get_supported_payment();

		std::uint64_t get_merchant_uid() const;

		awaitable<bool> check_repo_changed(std::string oldhead) const;
		awaitable<std::string> git_head() const;

		std::filesystem::path repo_path() const;

		static std::filesystem::path repo_path(std::filesystem::path repo_root_dir, std::uint64_t merchant_id);

	private:
		boost::asio::thread_pool& thread_pool;

		const merchant_git_repo_impl& impl() const;
		merchant_git_repo_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
