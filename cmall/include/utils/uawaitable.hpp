﻿//
// uawaitable.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/type_traits.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

namespace asio_util
{
	struct uawaitable_t
	{
		inline boost::asio::redirect_error_t<typename boost::decay<decltype(boost::asio::use_awaitable)>::type>
			operator[](boost::system::error_code& ec) const noexcept
		{
			return boost::asio::redirect_error(boost::asio::use_awaitable, ec);
		}
	};

	//
	// use_awaitable usage:
	//
	// boost::system::error_code ec;
	// stream.async_read(buffer, asio_util::use_awaitable[ec]);
	//

	constexpr uawaitable_t use_awaitable;
}

