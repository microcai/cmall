
#pragma once

#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

namespace services
{

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

		// 详情, markdown 或者 富文本.
		std::string detailed;

		std::string git_version;

		std::uint64_t merchant_id;
	};

	struct repo_products_impl;
	class repo_products
	{
	public:

		static bool init_bare_repo(boost::filesystem::path repo_path);

		static bool is_git_repo(boost::filesystem::path repo_path);

		repo_products(boost::asio::io_context& io, std::uint64_t merchant_id, boost::filesystem::path repo_path);
		~repo_products();

		boost::asio::awaitable<std::string> get_file_content(boost::filesystem::path path, boost::system::error_code& ec);

		// 扫描用户仓库, 返回找到的商品定义.
		boost::asio::awaitable<std::vector<product>> get_products();

		// 从给定的 goods_id 找到商品定义.
		boost::asio::awaitable<product> get_products(std::string goods_id, boost::system::error_code& ec);
		boost::asio::awaitable<product> get_products(std::string goods_id);

	private:
		const repo_products_impl& impl() const;
		repo_products_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
