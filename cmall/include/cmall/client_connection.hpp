
#pragma once

#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <vector>

#include "cmall/error_code.hpp"
#include "cmall/misc.hpp"
#include "services/verifycode.hpp"
#include "services/persist_session.hpp"
#include "services/payment_service.hpp"
#include "services/userscript_service.hpp"
#include "services/gitea_service.hpp"

#include "httpd/acceptor.hpp"
#include "httpd/http_stream.hpp"

#include "utils/time_clock.hpp"

namespace cmall {

	using ws_stream = websocket::stream<boost::beast::tcp_stream>;

	struct websocket_connection
	{
		websocket_connection(websocket_connection&& c) = delete;

		websocket_connection(httpd::http_any_stream& tcp_stream_)
			: ws_stream_(tcp_stream_)
			, message_channel(tcp_stream_.get_executor(), 1)
		{}

		void close(auto connection_id)
		{
			LOG_FMT("ws client close() called: [{}]", connection_id);

			boost::system::error_code ignore_ec;
			message_channel.close();
		}

		websocket::stream<httpd::http_any_stream&> ws_stream_;
		boost::asio::experimental::concurrent_channel<void(boost::system::error_code, std::string)> message_channel;
		bool m_disable_ping = false;
		std::string baseurl_;
	};

	struct client_connection : boost::noncopyable
	{
		httpd::http_any_stream tcp_stream;

		int64_t connection_id_;
		std::string remote_host_;
		std::string x_real_ip;

		std::uint64_t last_processed_req_id = { 0 };

		std::optional<websocket_connection> ws_client;

		std::shared_ptr<services::client_session> session_info;

		std::map<std::uint64_t, std::shared_ptr<boost::asio::cancellation_signal>> cancel_signals_;

		auto get_executor()
		{
			return tcp_stream.get_executor();
		}

		boost::asio::ip::tcp::socket& socket()
		{
			return tcp_stream.socket();
		}

		boost::asio::local::stream_protocol::socket& unix_socket()
		{
			return tcp_stream.unix_socket();
		}

		~client_connection()
		{
			for (auto & cs: cancel_signals_)
				cs.second->emit(boost::asio::cancellation_type::all);
			tcp_stream.close();
			LOG_DBG << (ws_client? "ws" : "http" ) <<  " client leave: " << connection_id_ << ", remote: " << x_real_ip;
		}

		void close()
		{
			for (auto & cs: cancel_signals_)
				cs.second->emit(boost::asio::cancellation_type::all);
			if (ws_client)
				ws_client->close(connection_id_);
			tcp_stream.close();
		}

		template<typename Executor>
		client_connection(Executor&& io, int64_t connection_id)
			: tcp_stream(boost::variant2::in_place_type<boost::beast::tcp_stream>, std::forward<Executor>(io))
			, connection_id_(connection_id)
		{
		}

		template<typename Executor>
		client_connection(Executor&& io, boost::asio::ssl::context& sslctx, int64_t connection_id)
			: tcp_stream(boost::variant2::in_place_type<boost::beast::ssl_stream<boost::beast::tcp_stream>>, std::forward<Executor>(io), sslctx)
			, connection_id_(connection_id)
		{
		}

		template<typename Executor>
		client_connection(Executor&& io, int64_t connection_id, int unix_stream)
			: tcp_stream(boost::variant2::in_place_type<httpd::unix_stream>, std::forward<Executor>(io))
			, connection_id_(connection_id)
		{
			boost::ignore_unused(unix_stream);
		}

	};

	using client_connection_ptr = std::shared_ptr<client_connection>;
	using client_connection_weakptr = std::weak_ptr<client_connection>;
}
