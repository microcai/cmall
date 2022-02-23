
#pragma once

#include "boost/asio/awaitable.hpp"
namespace cmall::vendor
{

	struct sms_config
	{
	};

	class sms
	{
	public:
		explicit sms(sms_config) { }
		~sms() { }

	public:
		boost::asio::awaitable<bool> send(const std::string& phone, std::any t) {
			co_return false;
		};
	};

}
