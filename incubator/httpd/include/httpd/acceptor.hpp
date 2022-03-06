
#pragma once


#include "boost/asio/cancellation_signal.hpp"
#include "boost/asio/error.hpp"
#include <iostream>
#include <mutex>
#if __has_include("utils/logging.hpp")
#include "utils/logging.hpp"
#define HTTPD_ENABLE_LOGGING 1
#endif

#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/associated_cancellation_slot.hpp"
#include "boost/asio/associated_executor.hpp"
#include "boost/asio/async_result.hpp"
#include "boost/asio/awaitable.hpp"
#include "boost/asio/bind_cancellation_slot.hpp"
#include "boost/asio/cancellation_type.hpp"
#include "boost/asio/co_spawn.hpp"
#include "boost/asio/coroutine.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/redirect_error.hpp"
#include "boost/asio/this_coro.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/system/detail/error_code.hpp"
#include "boost/system/system_error.hpp"
#include <exception>
#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <unordered_map>
#include <utility>

#include "detail/parser.hpp"
#include "detail/time_clock.hpp"

// 每线程一个 accept 的 多 socket accept 模式 适配器.

namespace httpd {

constexpr bool has_so_reuseport()
{
#ifdef __linux__
	return true;
#else
	return false;
#endif
}

namespace socket_options {
#ifdef SO_REUSEPORT
typedef boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port;
#endif
#ifdef IPV6_V6ONLY
typedef boost::asio::detail::socket_option::boolean<IPPROTO_IPV6, IPV6_V6ONLY> ipv6_only;
#endif
}

template<typename AcceptedClientClass, typename Execotor = boost::asio::any_io_executor>
class acceptor
{
    template<typename GetExecutor, typename ClientCreator, typename ClientRunner, typename CompletionHandler>
    struct delegated_operators : boost::noncopyable
    {
        boost::asio::cancellation_signal cancel_signal;
        CompletionHandler handler;
        ClientCreator creator;
        ClientRunner runner;
        GetExecutor get_executor_;

		bool invoked = false;

		void complete(boost::system::error_code ec)
        {
            if (!invoked)
            {
				invoked = true;
                auto executor = boost::asio::get_associated_executor(handler);
                boost::asio::post(executor, [ec, handler = std::move(this->handler)]() mutable { handler(ec); });
#ifdef HTTPD_ENABLE_LOGGING
                LOG_DBG << "delegated_operators complete called";
#endif
            }
        }

        auto get_executor()
        {
            return get_executor_();
        }

        auto make_client(std::size_t connection_id, std::string remote_addr, boost::beast::tcp_stream&& accepted_socket)
        {
            return creator(connection_id, remote_addr, std::move(accepted_socket));
        }

        auto start_runner(std::size_t connection_id, AcceptedClientClass client)
        {
            return runner(connection_id, client);
        }

        delegated_operators(CompletionHandler&& handler, ClientCreator&& creator, ClientRunner&& runner, GetExecutor&& get_executor_)
            : handler(std::move(handler))
            , creator(std::forward<ClientCreator>(creator))
            , runner(std::forward<ClientRunner>(runner))
            , get_executor_(std::forward<GetExecutor>(get_executor_))
        {}
    };


    boost::asio::ip::tcp::acceptor accept_socket;
    std::unordered_map<std::size_t, AcceptedClientClass> all_client;
    bool accepting = false;

public:
    acceptor(const acceptor&) = delete;
    acceptor(acceptor&& o)
        : accept_socket(std::move(o.accept_socket))
        , accepting(o.accepting)
    {
        BOOST_ASSERT_MSG(!o.accepting, "cannot move a socket that has pending IO");
    };

    template<typename AnyExecotor>
    explicit acceptor(AnyExecotor&& executor)
        : accept_socket(std::forward<AnyExecotor>(executor))
    {
    }

    template<typename AnyExecotor>
    explicit acceptor(AnyExecotor&& executor, std::string listen_address)
        : accept_socket(std::forward<AnyExecotor>(executor))
    {
        this->listen(listen_address);
    }

    auto get_executor()
    {
        return accept_socket.get_executor();
    }

    void listen(std::string listen_address)
    {
        boost::system::error_code ec;
        this->listen(listen_address, ec);
        if (ec)
            throw boost::system::system_error(ec);
    }

    void listen(std::string listen_address, boost::system::error_code& ec)
    {
        using tcp = boost::asio::ip::tcp;
        tcp::endpoint endp;

        bool ipv6only = httpd::detail::make_listen_endpoint(listen_address, endp, ec);
        if (ec)
        {
#ifdef HTTPD_ENABLE_LOGGING
            LOG_ERR << "WS server listen error: " << listen_address << ", ec: " << ec.message();
#endif
            return;
        }


        accept_socket.open(endp.protocol(), ec);
        if (ec)
        {
#ifdef HTTPD_ENABLE_LOGGING
            LOG_ERR << "WS server open accept error: " << ec.message();
#endif
            return;
        }
#ifdef SO_REUSEPORT
        if constexpr (has_so_reuseport())
        {
            accept_socket.set_option(socket_options::reuse_port(true), ec);
            if (ec)
            {
#ifdef HTTPD_ENABLE_LOGGING
                LOG_ERR << "WS server accept set option SO_REUSEPORT failed: " << ec.message();
#endif
                return;
            }
        }
#endif
        accept_socket.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
        {
#ifdef HTTPD_ENABLE_LOGGING
            LOG_ERR << "WS server accept set option failed: " << ec.message();
#endif
            return;
        }

#if __linux__
        if (ipv6only)
        {
            int on = 1;
            if (::setsockopt(accept_socket.native_handle(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&on, sizeof(on)) == -1)
            {
#ifdef HTTPD_ENABLE_LOGGING
                LOG_ERR << "WS server setsockopt IPV6_V6ONLY";
#endif
                return;
            }
        }
#else
        boost::ignore_unused(ipv6only);
#endif
        accept_socket.bind(endp, ec);
        if (ec)
        {
#ifdef HTTPD_ENABLE_LOGGING
            LOG_ERR << "WS server bind failed: " << ec.message() << ", address: " << endp.address().to_string()
                    << ", port: " << endp.port();
#endif
            return;
        }

        accept_socket.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
        {
#ifdef HTTPD_ENABLE_LOGGING
            LOG_ERR << "WS server listen failed: " << ec.message();
#endif
            return;
        }
    }

    template<typename ClientClassCreator, typename ClientRunner, typename GetExecutor>
    boost::asio::awaitable<void> run_accept_loop(int number_of_concurrent_acceptor, ClientClassCreator&& creator, ClientRunner&& runner, GetExecutor&& get_executor_)
    {
        return boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void(boost::system::error_code)>(
            [this, number_of_concurrent_acceptor
                , creator = std::forward<ClientClassCreator>(creator)
                , runner = std::forward<ClientRunner>(runner)
                , get_executor_ = std::forward<GetExecutor>(get_executor_)](auto&& handler) mutable
            {
                typedef delegated_operators<GetExecutor, ClientClassCreator, ClientRunner, std::decay_t<decltype(handler)>> delegated_operators_t;
                boost::asio::cancellation_slot cs = boost::asio::get_associated_cancellation_slot(handler);
                auto creator_waiter = std::make_shared<delegated_operators_t>
                    (std::move(handler), std::forward<ClientClassCreator>(creator), std::forward<ClientRunner>(runner), std::forward<GetExecutor>(get_executor_));
                if (cs.is_connected())
                {
                    cs.assign([creator_waiter = std::weak_ptr<delegated_operators_t>(creator_waiter)](boost::asio::cancellation_type_t t) mutable {
                        if (creator_waiter.lock())
                                creator_waiter.lock()->cancel_signal.emit(t);
                    });
                }
                for(int i =0; i < number_of_concurrent_acceptor; i++)
                {
                    boost::asio::co_spawn(this->get_executor(), accept_loop(*creator_waiter), [creator_waiter](std::exception_ptr p)
                    {
                        if (creator_waiter->invoked)
                            return;
                        creator_waiter->complete(boost::asio::error::operation_aborted);
                    });
                }
            }
            , boost::asio::use_awaitable
        );
    }

    boost::asio::awaitable<void> clean_shutdown()
    {

        for (auto & ws: all_client)
        {
            auto& conn_ptr = ws.second;
            conn_ptr->close();
        }

		while (all_client.size())
		{
            using timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;
			timer t(get_executor());
			t.expires_from_now(std::chrono::milliseconds(20));
			co_await t.async_wait(boost::asio::use_awaitable);
		}
        co_return ;
    }

private:
    template<typename A, typename  B, typename C, typename D>
    boost::asio::awaitable<void> accept_loop(delegated_operators<A, B, C, D>& delegated_operator)
    {
		while (true)
        {
            boost::system::error_code error;
#ifdef SO_REUSEPORT
            boost::asio::ip::tcp::socket client_socket(get_executor());
#else
            boost::asio::ip::tcp::socket client_socket(delegated_operator.get_executor());
#endif
            co_await accept_socket.async_accept(client_socket, boost::asio::bind_cancellation_slot(delegated_operator.cancel_signal.slot(), boost::asio::use_awaitable));

            client_socket.set_option(boost::asio::socket_base::keep_alive(true), error);
            boost::beast::tcp_stream stream(std::move(client_socket));

            static std::atomic_size_t id{ 0 };
            size_t connection_id = id++;
            std::string remote_host;
            auto endp = stream.socket().remote_endpoint(error);
            if (!error)
            {
                if (endp.address().is_v6())
                {
                    remote_host = "[" + endp.address().to_string() + "]:" + std::to_string(endp.port());
                }
                else
                {
                    remote_host = endp.address().to_string() + ":" + std::to_string(endp.port());
                }
            }

            auto client_ptr = co_await delegated_operator.make_client(connection_id, remote_host, std::move(stream));

            all_client.emplace(connection_id, client_ptr);

            boost::asio::co_spawn(client_ptr->get_executor(),
                delegated_operator.start_runner(connection_id, client_ptr),
                [this, connection_id, client_ptr](std::exception_ptr)
                {
                    all_client.erase(connection_id);
                });

        }
    }

};

}