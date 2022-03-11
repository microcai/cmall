
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <chrono>
#include <cstdlib>

#include "boost/asio/use_awaitable.hpp"
#include "boost/process/search_path.hpp"
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

		boost::asio::awaitable<payment_url> get_payurl(std::string_view script_content, std::string orderid, int nthTry, std::string order_title, std::string order_amount, PAYMENT_GATEWAY gateway)
		{
			payment_url ret;
			using namespace boost::process;
			if (gateway == PAYMENT_CHSPAY)
			{
				async_pipe nodejs_output(io);
				async_pipe nodejs_input(io);

				child cp(io, search_path("node"), "-", "--", "--order-id", orderid, "--order-amount", order_amount, std_out > nodejs_output, std_in < nodejs_input, start_dir("/tmp"));

				co_await boost::asio::async_write(nodejs_input, boost::asio::buffer(script_content), boost::asio::use_awaitable);

				std::string out;
				out.resize(4096);

				auto out_size = co_await boost::asio::async_read(nodejs_output, boost::asio::buffer(out), boost::asio::use_awaitable);
				out.resize(out_size);

				std::error_code stdec;
				cp.wait_for(std::chrono::milliseconds(12), stdec);
				if (!cp.running())
					cp.join();

				int result = cp.exit_code();
				if (result == EXIT_SUCCESS)
				{
					ret.uri = out;
				}
			}
			co_return ret;
		}

		boost::asio::awaitable<bool> query_pay(std::string orderid, PAYMENT_GATEWAY gateway)
		{
			using namespace boost::process;
			if (gateway == PAYMENT_CHSPAY)
			{
				async_pipe ap(io);
				child cp(search_path("chstx"), "query_pay", orderid, std_out > ap);

				std::string out;
				out.resize(4096);

				auto out_size = co_await boost::asio::async_read(ap, boost::asio::buffer(out), boost::asio::use_awaitable);
				out.resize(out_size);

				std::error_code stdec;
				cp.wait_for(std::chrono::milliseconds(12), stdec);
				if (!cp.running())
					cp.join();

				int result = cp.exit_code();
				co_return result == EXIT_SUCCESS;
			}
			co_return false;
		}

		boost::asio::io_context& io;
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
