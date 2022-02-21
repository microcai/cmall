//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include "cmall/internal.hpp"
#include "cmall/database.hpp"

#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>

#include "jsonrpc.hpp"

#include "cmall/error_code.hpp"

namespace cmall {

	struct server_config
	{
		std::vector<std::string> upstreams_;
		std::vector<std::string> ws_listens_;
		std::string wsorigin_;
		std::string socks5proxy_;

		cmall::db_config dbcfg_;
	};

	using ws_stream = websocket::stream<boost::beast::tcp_stream>;


	struct websocket_connection
	{
		websocket_connection(websocket_connection&& c) = delete;

		websocket_connection(boost::beast::tcp_stream& tcp_stream_)
			: ws_stream_(tcp_stream_)
			, message_channel(tcp_stream_.get_executor(), 1)
		{}

		void close(auto connection_id)
		{
			LOG_DBG << "ws client close() called: [" << connection_id << "]";
			message_channel.cancel();
		}

		websocket::stream<boost::beast::tcp_stream&> ws_stream_;
		boost::asio::experimental::concurrent_channel<void(boost::system::error_code, std::string)> message_channel;
	};

	struct client_connection
	{
		boost::beast::tcp_stream tcp_stream;
		boost::asio::any_io_executor m_io;
		int64_t connection_id_;
		std::string remote_host_;

		std::optional<websocket_connection> ws_client;

		~client_connection()
		{
			LOG_DBG << (ws_client? "ws" : "http" ) <<  " client leave: " << connection_id_ << ", remote: " << remote_host_;
		}

		void close()
		{
			if (ws_client)
				ws_client->close(connection_id_);
			boost::beast::close_socket(tcp_stream);
		}

		client_connection(boost::beast::tcp_stream&& ws, int64_t connection_id, const std::string& remote_host)
			: tcp_stream(std::move(ws))
			, m_io(tcp_stream.get_executor())
			, connection_id_(connection_id)
			, remote_host_(remote_host)
		{
		}
	};

	using client_connection_ptr = std::shared_ptr<client_connection>;
	using client_connection_weakptr = std::weak_ptr<client_connection>;

	class cmall_service
	{
		// c++11 noncopyable.
		cmall_service(const cmall_service&) = delete;
		cmall_service& operator=(const cmall_service&) = delete;

		using executor_type = boost::asio::any_io_executor;

	public:
		cmall_service(io_context_pool& ios, const server_config& config);
		~cmall_service();

	public:
		boost::asio::awaitable<int> run_httpd();

		boost::asio::awaitable<void> stop();

	private:
		boost::asio::awaitable<bool> init_ws_acceptors();

		boost::asio::awaitable<void> listen_loop(tcp::acceptor& a);
		boost::asio::awaitable<void> handle_accepted_client(size_t connection_id, client_connection_ptr);

		boost::asio::awaitable<int> do_http_handle(size_t connection_id, boost::beast::http::request<boost::beast::http::string_body>&, boost::beast::tcp_stream& client);
		boost::asio::awaitable<void> do_ws_read(size_t connection_id, client_connection_ptr);
		boost::asio::awaitable<void> do_ws_write(size_t connection_id, client_connection_ptr);

		client_connection_ptr add_ws(size_t connection_id, const std::string& remote_host, boost::beast::tcp_stream&& tcp_stream);
		void remove_ws(size_t connection_id);
		boost::asio::awaitable<void> close_all_ws();

		boost::asio::awaitable<void> websocket_write(client_connection_ptr connection_ptr, std::string message);

		boost::asio::awaitable<void> notify_message_to_all_client(std::string message);

		boost::asio::awaitable<void> mitigate_chaindb();

		// 换算成人类读取单位.
		boost::asio::awaitable<boost::json::object> on_client_invoke(size_t connection_id, const std::string& method, boost::json::value jv);

	private:
		boost::asio::awaitable<std::shared_ptr<ws_stream>> connect(size_t index = 0);

	private:
		io_context_pool& m_io_context_pool;
		boost::asio::io_context& m_io_context;

		server_config m_config;
		cmall_database m_database;

		// ws 服务端相关.
		std::vector<tcp::acceptor> m_ws_acceptors;
		std::mutex m_ws_mux;
		std::unordered_map<size_t, client_connection_ptr> m_ws_streams;

		std::atomic_bool m_abort{false};
	};
}
