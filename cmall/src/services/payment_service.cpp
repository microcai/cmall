
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
#include "cmall/error_code.hpp"

#include "../sandbox.hpp"

namespace services
{
	struct payment_impl
	{
		payment_impl(boost::asio::io_context& io)
			: nodejs(io)
		{}

		boost::asio::awaitable<payment_url> get_payurl(std::string_view script_content, std::string orderid, int nthTry, std::string order_amount, std::string paymentmethod)
		{
			payment_url ret;

			if (script_content.empty())
			{
				throw boost::system::system_error(cmall::error::no_payment_script_supplyed);
			}

			ret.uri = co_await nodejs.run_script(script_content, {"--order-id", orderid, "--order-amount", order_amount, "--method", paymentmethod});
			if (!ret.uri.empty())
				ret.uri.pop_back();
			co_return ret;
		}

		services::userscript nodejs;
	};

	// verify the user input verify_code against verify_session
	boost::asio::awaitable<payment_url> payment::get_payurl(std::string_view script_content, std::string orderid, int nthTry, std::string order_amount, std::string paymentmethod)
	{
		return impl().get_payurl(script_content, orderid, nthTry, order_amount, paymentmethod);
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
