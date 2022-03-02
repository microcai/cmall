
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/payment_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

namespace services
{
	struct payment_impl
	{
		payment_impl(boost::asio::io_context& io)
			: io(io)
		{}

		boost::asio::awaitable<payment_url> get_payurl(std::string orderid, int nthTry, std::string order_title, std::string order_amount, PAYMENT_GATEWAY gateway)
		{
			payment_url ret;
			ret.uri = "com.rncoldwallet://pay/HandleTransaction?to=1&value=2&tokenData=3";
			co_return ret;
		}

		boost::asio::io_context& io;
	};

	// verify the user input verify_code against verify_session
	boost::asio::awaitable<payment_url> payment::get_payurl(std::string orderid, int nthTry, std::string order_title, std::string order_amount, PAYMENT_GATEWAY gateway)
	{
		return impl().get_payurl(orderid, nthTry, order_title, order_amount, gateway);
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
