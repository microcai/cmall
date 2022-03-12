
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <boost/process/extend.hpp>
#include <boost/process/handles.hpp>
#include <chrono>
#include <cstdlib>

#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost::asio::experimental::awaitable_operators;

#include "services/payment_service.hpp"
#include "services/userscript_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

#include "../sandbox.hpp"

namespace services
{
	struct payment_impl
	{
		payment_impl(boost::asio::io_context& io)
			: nodejs(io)
		{}

		boost::asio::awaitable<payment_url> get_payurl(std::string_view script_content, std::string orderid, int nthTry, std::string order_title, std::string order_amount, PAYMENT_GATEWAY gateway)
		{
			payment_url ret;
			ret.uri = co_await nodejs.run_script(script_content, {"-", "--", "--order-id", orderid, "--order-amount", order_amount});
			if (!ret.uri.empty())
				ret.uri.pop_back();
			co_return ret;
		}

		boost::asio::awaitable<bool> query_pay(std::string orderid, PAYMENT_GATEWAY gateway)
		{
			auto result = co_await nodejs.run_script("", {"-", "--", "--order-id", orderid});
			co_return result == "payed";
		}

		services::userscript nodejs;
	};

	// verify the user input verify_code against verify_session
	boost::asio::awaitable<payment_url> payment::get_payurl(std::string_view script_content, std::string orderid, int nthTry, std::string order_title, std::string order_amount, PAYMENT_GATEWAY gateway)
	{
		return impl().get_payurl(script_content, orderid, nthTry, order_title, order_amount, gateway);
	}

	boost::asio::awaitable<bool> payment::query_pay(std::string orderid, PAYMENT_GATEWAY gateway)
	{
		return impl().query_pay(orderid, gateway);
	}

	payment::payment(boost::asio::io_context& io)
	{
		static_assert(sizeof(obj_stor) >= sizeof(payment_impl));
		std::construct_at(reinterpret_cast<payment_impl*>(obj_stor.data()), io);
	}

	payment::~payment()
	{
		std::destroy_at(reinterpret_cast<payment_impl*>(obj_stor.data()));
	}

	const payment_impl& payment::impl() const
	{
		return *reinterpret_cast<const payment_impl*>(obj_stor.data());
	}

	payment_impl& payment::impl()
	{
		return *reinterpret_cast<payment_impl*>(obj_stor.data());
	}

}
