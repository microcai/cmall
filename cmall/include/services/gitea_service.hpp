
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

namespace services
{
	struct gitea_impl;
	class gitea
	{
	public:
		gitea(boost::asio::io_context& io);
		~gitea();

		// 生成用户.
		boost::asio::awaitable<bool> init_user(std::uint64_t uid, const std::string& password, const std::string& template_dir);
		// 修改密码
		boost::asio::awaitable<bool> change_password(std::uint64_t uid, const std::string& password);

	private:
		const gitea_impl& impl() const;
		gitea_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
