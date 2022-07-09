
#include "stdafx.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/neteasesdk.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;

namespace services
{
	struct neteasesdk_impl
	{
		neteasesdk_impl(boost::asio::io_context& io)
			: io(io)
		{}

		// verify the user input verify_code against verify_session
		awaitable<std::string> get_user_phone(std::string token, std::string accessToken)
		{
			co_return "";
		}

		boost::asio::io_context& io;
	};

	// verify the user input verify_code against verify_session
	awaitable<std::string> neteasesdk::get_user_phone(std::string token, std::string key)
	{
		return impl().get_user_phone(token, key);
	}

	neteasesdk::neteasesdk(boost::asio::io_context& io)
	{
		static_assert(sizeof(obj_stor) >= sizeof(neteasesdk_impl));
		std::construct_at(reinterpret_cast<neteasesdk_impl*>(obj_stor.data()), io);
	}

	neteasesdk::~neteasesdk()
	{
		std::destroy_at(reinterpret_cast<neteasesdk_impl*>(obj_stor.data()));
	}

	const neteasesdk_impl& neteasesdk::impl() const
	{
		return *reinterpret_cast<const neteasesdk_impl*>(obj_stor.data());
	}

	neteasesdk_impl& neteasesdk::impl()
	{
		return *reinterpret_cast<neteasesdk_impl*>(obj_stor.data());
	}

}
