
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

#include "services/userscript_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

#include "../sandbox.hpp"

namespace services
{
	struct userscript_impl
	{
		userscript_impl(boost::asio::io_context& io)
			: io(io)
		{}

		boost::asio::awaitable<std::string> run_script(std::string_view script_content, std::vector<std::string> script_arguments)
		{
			using namespace boost::process;

			async_pipe nodejs_output(io);
			async_pipe nodejs_input(io);

			LOG_DBG << "script_content = " << script_content;

			child cp(io, search_path("node"), boost::process::args(script_arguments)
				, std_in < nodejs_input, std_out > nodejs_output, start_dir("/tmp")
#ifdef __linux
				, boost::process::extend::on_exec_setup=[](auto & exec) { sandbox::no_fd_leak(); sandbox::install_seccomp(); sandbox::drop_root(); }
#endif
			);

			std::string out;
			auto d_buffer = boost::asio::dynamic_buffer(out);

			auto read_promis = boost::asio::async_read_until(nodejs_output, d_buffer, '\n', boost::asio::experimental::use_promise);

			std::string drop_privalage = "try{chroot('/');process.setgid(65535); process.setuid(65535);}catch(e){}\n";

			//co_await boost::asio::async_write(nodejs_input, boost::asio::buffer(drop_privalage), boost::asio::use_awaitable);
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
			if (cp.running())
				cp.terminate();

			int result = cp.exit_code();
			co_return out;
		}

		boost::asio::io_context& io;
	};

	boost::asio::awaitable<std::string> userscript::run_script(std::string_view script_file_content, std::vector<std::string> script_arguments)
	{
		return impl().run_script(script_file_content, script_arguments);
	}

	userscript::userscript(boost::asio::io_context& io)
	{
		static_assert(sizeof(obj_stor) >= sizeof(userscript_impl));
		std::construct_at(reinterpret_cast<userscript_impl*>(obj_stor.data()), io);
	}

	userscript::~userscript()
	{
		std::destroy_at(reinterpret_cast<userscript_impl*>(obj_stor.data()));
	}

	const userscript_impl& userscript::impl() const
	{
		return *reinterpret_cast<const userscript_impl*>(obj_stor.data());
	}

	userscript_impl& userscript::impl()
	{
		return *reinterpret_cast<userscript_impl*>(obj_stor.data());
	}

}
