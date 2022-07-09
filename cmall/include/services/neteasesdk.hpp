﻿
#pragma once

#include <memory>
#include <boost/asio.hpp>

using boost::asio::awaitable;

namespace services
{
	struct neteasesdk_impl;
	class neteasesdk
	{
	public:
		neteasesdk(boost::asio::io_context& io);
		~neteasesdk();

		// 使用前端传来的 token 获得用户真正的 手机号
		awaitable<std::string> get_user_phone(std::string token, std::string key);

	private:
		const neteasesdk_impl& impl() const;
		neteasesdk_impl& impl();

		std::array<char, 512> obj_stor;


	};
}
