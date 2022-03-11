
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <boost/process/extend.hpp>
#include <chrono>
#include <cstdlib>

#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost::asio::experimental::awaitable_operators;

#include "services/payment_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

#include "../sandbox.hpp"

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

				LOG_DBG << "parent pid = " << getpid();

				child cp(search_path("node"), "-", "--", "--order-id", orderid, "--order-amount", order_amount
					, std_in < nodejs_input, std_out > nodejs_output, start_dir("/tmp"), limit_handles.allowStd(),
#ifdef __linux
					, boost::process::extend::on_exec_setup=[](auto & exec) { sandbox::no_fd_leak(); sandbox::install_seccomp(); sandbox::drop_root(); }
#endif
				);

				std::string out;
				auto d_buffer = boost::asio::dynamic_buffer(out);

				auto read_promis = boost::asio::async_read_until(nodejs_output, d_buffer, '\n', boost::asio::experimental::use_promise);

				std::string drop_privalage = "try{chroot('/');process.setgid(65535); process.setuid(65535);}catch(e){}\n";

				auto new_script = std::string("") + std::string(script_content);

				co_await boost::asio::async_write(nodejs_input, boost::asio::buffer(drop_privalage), boost::asio::use_awaitable);
				co_await boost::asio::async_write(nodejs_input, boost::asio::buffer(script_content), boost::asio::use_awaitable);
				nodejs_input.close();

				boost::asio::basic_waitable_timer<time_clock::steady_clock>  t(co_await boost::asio::this_coro::executor);
				t.expires_from_now(std::chrono::seconds(20));

				auto out_size = co_await (
					read_promis.async_wait(boost::asio::use_awaitable) || t.async_wait(boost::asio::use_awaitable)
				);

				out.resize(std::get<0>(out_size));

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
