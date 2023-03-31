
#include "stdafx.hpp"

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
#include "utils/scoped_exit.hpp"
#include "utils/uawaitable.hpp"
#include "utils/coroyield.hpp"

#include "cmall/error_code.hpp"
#include "sandbox.hpp"

using boost::asio::use_awaitable;
using boost::asio::experimental::use_promise;

#ifdef __linux__

#include <sys/socket.h>

static int recv_fd(int sock)
{
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	char buf[CMSG_SPACE(sizeof(int))] = {0}, c = 'c';
	struct iovec io = {
		.iov_base = &c,
		.iov_len = 1,
	};

	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);

	if (recvmsg(sock, &msg, 0) < 0) {
		perror("recvmsg");
		return -1;
	}

	cmsg = CMSG_FIRSTHDR(&msg);

	return *((int *)CMSG_DATA(cmsg));
}
#endif

template <typename MutableBuffers>
awaitable<std::size_t> read_all_pipe_data(boost::process::async_pipe& pipe, MutableBuffers&& buffers)
{
	std::size_t total_transfred = 0;
	boost::system::error_code ec;
	do
	{
		auto buf = buffers.prepare(512);
		std::size_t bytes_transfered = co_await pipe.async_read_some(buf, asio_util::use_awaitable[ec]);
		if (ec)
		{
			if (bytes_transfered > 0)
			{
				total_transfred += bytes_transfered;
				buffers.commit(bytes_transfered);
			}
			co_return total_transfred;
		}
		if (bytes_transfered == 0)
			co_return total_transfred;

		total_transfred += bytes_transfered;
		buffers.commit(bytes_transfered);
	}while(true);
}

namespace services
{
	struct userscript_impl
	{
		userscript_impl(boost::asio::io_context& io)
			: io(io)
		{}

		awaitable<std::string> run_script(std::string_view script_content, std::vector<std::string> script_arguments)
		{
			co_await this_coro::coro_yield();
			using namespace boost::process;

			async_pipe nodejs_output(io);
			async_pipe nodejs_input(io);

			std::vector<std::string> node_args;

#ifdef __linux__
			sandbox::sandboxVFS vfs;

			using boost::asio::local::datagram_protocol;
			datagram_protocol::socket fd_recive_socket(io);
			datagram_protocol::socket fd_sending_socket(io);
			boost::asio::local::connect_pair(fd_recive_socket, fd_sending_socket);

			// 为了不 hook access/fstat 这些调用， 就用系统已经存在的，也必然存在的文件名.
			vfs.insert({"/proc/cmdline", script_content});
			node_args.push_back("/proc/cmdline");

			std::copy(script_arguments.begin(), script_arguments.end(), std::back_inserter(node_args));

#elif defined(_WIN32)
			node_args.push_back("-");
			std::copy(script_arguments.begin(), script_arguments.end(), std::back_inserter(node_args));
#endif
			child cp(io, search_path("node"), boost::process::args(node_args)
				, std_out > nodejs_output, std_in < nodejs_input
#ifdef _WIN32
				, start_dir("C:\\")
#endif
#ifdef __linux__
				, start_dir("/tmp"),
				boost::process::extend::on_exec_setup=[&fd_recive_socket, &fd_sending_socket](auto & exec)
				{
					fd_recive_socket.close();
					sandbox::no_fd_leak();
					sandbox::install_seccomp(fd_sending_socket.release());
					sandbox::drop_root();
				}
#endif
			);

#ifdef __linux__
			fd_sending_socket.close();
			int sec_comp_notify_fd_ = recv_fd(fd_recive_socket.native_handle());
			fd_recive_socket.close();

			sandbox::supervisor supervisor(boost::asio::posix::stream_descriptor(co_await boost::asio::this_coro::executor, sec_comp_notify_fd_), vfs);

			auto seccomp_supervisor_promise = boost::asio::co_spawn(co_await boost::asio::this_coro::executor,
				supervisor.start_supervisor(), use_promise);
#endif
			std::string out;
			auto d_buffer = boost::asio::dynamic_buffer(out);

			auto read_promis = boost::asio::co_spawn(io, read_all_pipe_data(nodejs_output, d_buffer), use_promise);

#ifdef _WIN32
			co_await boost::asio::async_write(nodejs_input, boost::asio::buffer(script_content), use_awaitable);
#endif
			nodejs_input.close();

			awaitable_timer  t(co_await boost::asio::this_coro::executor);
			t.expires_from_now(std::chrono::seconds(5));

			auto out_size = co_await (
				read_promis(use_awaitable) || t.async_wait()
			);
			if (out_size.index() == 0)
			{
				out.resize(std::get<0>(out_size));
			}
			else
			{
				LOG_ERR << "script timedout, terminationg";
				// script timedout
				std::error_code stdec;
				cp.terminate(stdec);
				cp.wait(stdec);
				co_return "";
			}
			std::error_code stdec;
			cp.wait_for(std::chrono::milliseconds(12), stdec);
			if (cp.running())
			{
				cp.terminate();
			}
#ifdef __linux__
			seccomp_supervisor_promise.cancel();;
#endif
			cp.wait(stdec);
			co_return out;
		}

		awaitable<std::string> run_script(std::string_view script_file_content, std::string_view http_request_body,
			std::map<std::string, std::string> scriptenv, std::vector<std::string> script_arguments)
		{
			co_await this_coro::coro_yield();
		try {
			using namespace boost::process;

			async_pipe nodejs_output(io);
			async_pipe nodejs_input(io);

			std::vector<std::string> node_args;

			boost::process::environment nodejs_env;

			for (auto [k,v] : scriptenv)
				nodejs_env.emplace(k, v);

#ifdef __linux
			sandbox::sandboxVFS vfs;

			using boost::asio::local::datagram_protocol;
			datagram_protocol::socket fd_recive_socket(io);
			datagram_protocol::socket fd_sending_socket(io);
			boost::asio::local::connect_pair(fd_recive_socket, fd_sending_socket);

			// 为了不 hook access/fstat 这些调用， 就用系统已经存在的，也必然存在的文件名.
			vfs.insert({"/proc/cmdline", script_file_content});
			node_args.push_back("/proc/cmdline");

			std::copy(script_arguments.begin(), script_arguments.end(), std::back_inserter(node_args));

#elif defined(_WIN32)
			char * tmpfile_name_ = tempnam(std::filesystem::temp_directory_path().string().c_str(), NULL);
			std::string tmpfile_name(tmpfile_name_);
			free(tmpfile_name_);

			std::ofstream tmp_ofstream(tmpfile_name);
			tmp_ofstream.write(script_file_content.data(), script_file_content.length());
			tmp_ofstream.close();

			scoped_exit cleanup([tmpfile_name](){
				std::filesystem::remove(tmpfile_name);
			});

			node_args.push_back(tmpfile_name);
			std::copy(script_arguments.begin(), script_arguments.end(), std::back_inserter(node_args));
#endif
			child cp(io, search_path("node"), boost::process::args(node_args)
				, std_out > nodejs_output, std_in < nodejs_input, nodejs_env
				, start_dir(std::filesystem::temp_directory_path().string())
#ifdef __linux
				, boost::process::extend::on_exec_setup=[&fd_recive_socket, &fd_sending_socket](auto & exec)
				{
					fd_recive_socket.close();
					sandbox::install_seccomp(fd_sending_socket.release());
					sandbox::no_fd_leak();
					sandbox::drop_root();
				}
#endif
			);

#ifdef __linux__
			fd_sending_socket.close();
			int sec_comp_notify_fd_ = recv_fd(fd_recive_socket.native_handle());
			fd_recive_socket.close();

			sandbox::supervisor supervisor(boost::asio::posix::stream_descriptor(co_await boost::asio::this_coro::executor, sec_comp_notify_fd_), vfs);

			auto seccomp_supervisor_promise = boost::asio::co_spawn(co_await boost::asio::this_coro::executor,
				supervisor.start_supervisor(), use_promise);
#endif
			std::string out;
			auto d_buffer = boost::asio::dynamic_buffer(out);

			nodejs_output.native_source();

			auto read_promis = boost::asio::co_spawn(io, read_all_pipe_data(nodejs_output, d_buffer), use_promise);

			auto stdin_feader = boost::asio::co_spawn(io, [&]()-> awaitable<void>{
				co_await boost::asio::async_write(nodejs_input, boost::asio::buffer(http_request_body.data(), http_request_body.length()), use_awaitable);
				nodejs_input.async_close();
			}, use_promise);

			awaitable_timer  t(co_await boost::asio::this_coro::executor);
			t.expires_from_now(std::chrono::seconds(20));

			auto out_size = co_await (
				read_promis(use_awaitable) || t.async_wait()
			);
			if (out_size.index() == 0)
			{
				out.resize(std::get<0>(out_size));
			}
			else
			{
				LOG_ERR << "script timedout, terminationg";
				// script timedout
				std::error_code stdec;
				cp.terminate(stdec);
				cp.wait(stdec);
				co_return "";
			}
			std::error_code stdec;
			cp.wait_for(std::chrono::milliseconds(12), stdec);
			if (cp.running())
			{
				cp.terminate();
			}
#ifdef __linux__
			seccomp_supervisor_promise.cancel();;
#endif
			cp.wait(stdec);
			co_return out;
		}catch(std::exception&){}
			co_return "";
		}

		boost::asio::io_context& io;
	};

	awaitable<std::string> userscript::run_script(std::string_view script_file_content, std::vector<std::string> script_arguments)
	{
		co_return co_await  impl().run_script(script_file_content, script_arguments);
	}

	awaitable<std::string> userscript::run_script(
			std::string_view script_file_content,
			std::string_view http_request_body,
			std::map<std::string, std::string> scriptenv,
			std::vector<std::string> script_arguments)
	{
		co_return co_await impl().run_script(script_file_content, http_request_body, scriptenv, script_arguments);
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
