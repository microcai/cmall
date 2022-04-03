
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

using boost::asio::awaitable;

namespace services
{
	struct gitea_impl;
	class gitea
	{
	public:
		gitea(const std::string& gitea_api, const std::string& token);
		~gitea();

		// 生成用户.
		awaitable<bool> init_user(std::uint64_t uid, std::string password, std::string template_owner, std::string template_repo);
		// 修改密码
		awaitable<bool> change_password(std::uint64_t uid, std::string password);

	private:
		const gitea_impl& impl() const;
		gitea_impl& impl();

		std::array<char, 512> obj_stor;
	};
}
