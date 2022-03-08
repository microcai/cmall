//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "boost/asio/thread_pool.hpp"
#include "cmall/internal.hpp"
#include "cmall/database.hpp"

#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <vector>

#include "cmall/error_code.hpp"
#include "cmall/misc.hpp"
#include "persist_map.hpp"
#include "services/verifycode.hpp"
#include "services/persist_session.hpp"
#include "services/payment_service.hpp"
#include "services/repo_products.hpp"

#include "httpd/acceptor.hpp"

namespace cmall {

	struct server_config
	{
		std::vector<std::string> upstreams_;
		std::vector<std::string> ws_listens_;
		std::string wsorigin_;
		std::string socks5proxy_;

		cmall::db_config dbcfg_;

		std::filesystem::path session_cache_file;
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

	struct client_connection : boost::noncopyable
	{
		boost::beast::tcp_stream tcp_stream;
		int64_t connection_id_;
		std::string remote_host_;

		std::optional<websocket_connection> ws_client;

		std::shared_ptr<services::client_session> session_info;

		auto get_executor()
		{
			return tcp_stream.get_executor();
		}

		~client_connection()
		{
			boost::system::error_code ignore_ec;
			auto& sock  = boost::beast::get_lowest_layer(tcp_stream).socket();
			sock.close(ignore_ec);
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
			, connection_id_(connection_id)
			, remote_host_(remote_host)
		{
		}

	};

	using client_connection_ptr = std::shared_ptr<client_connection>;
	using client_connection_weakptr = std::weak_ptr<client_connection>;

	inline int64_t client_connection_get_id(client_connection* p)
	{
		return p->connection_id_;
	}

	inline uint64_t client_connection_get_user_id(client_connection* p)
	{
		return p->session_info->user_info->uid_;
	}

	// 这个多索引map 用来快速找到同一个用户的 session
	typedef boost::multi_index_container<
		client_connection*,
		boost::multi_index::indexed_by<
			boost::multi_index::sequenced<>,
			boost::multi_index::hashed_unique<
				boost::multi_index::global_fun<client_connection*, int64_t, &client_connection_get_id>
			>,
			boost::multi_index::hashed_non_unique<
				boost::multi_index::global_fun<client_connection*, uint64_t, &client_connection_get_user_id>
			>
		>
	> active_session_map;

	enum class req_method {
		recover_session,

		user_prelogin,
		user_login,
		user_logout,
		user_islogin,
		user_info,
		user_apply_merchant,

		user_list_recipient_address,
		user_add_recipient_address,
		user_modify_receipt_address,
		user_erase_receipt_address,

		cart_add,
		cart_mod,
		cart_del,
		cart_list,

		order_create_cart,
		order_create_direct,
		order_status,
		order_list,
		order_close,

		order_get_pay_url,

		goods_list,
		goods_detail,

		merchant_product_list,
		merchant_product_add,
		merchant_product_mod,
		merchant_product_launch,
		merchant_product_withdraw,

		admin_user_list,
		admin_user_ban,
		admin_merchant_list,
		admin_merchant_ban,
		admin_product_list,
		admin_product_withdraw,
		admin_order_force_refund,
	};

	class cmall_service
	{
		// c++11 noncopyable.
		cmall_service(cmall_service&&) = delete;
		cmall_service(const cmall_service&) = delete;
		cmall_service& operator=(const cmall_service&) = delete;

		using executor_type = boost::asio::any_io_executor;

	public:
		cmall_service(io_context_pool& ios, const server_config& config);
		~cmall_service();

	public:
		boost::asio::awaitable<bool> load_configs();

		boost::asio::awaitable<void> run_httpd();

		boost::asio::awaitable<void> stop();

	private:
		boost::asio::awaitable<bool> init_ws_acceptors();

		boost::asio::awaitable<void> handle_accepted_client(client_connection_ptr);

		boost::asio::awaitable<int> render_git_repo_files(size_t connection_id, std::string merchant, std::string path_in_repo, boost::beast::tcp_stream& client, boost::beast::http::request<boost::beast::http::string_body>);
		boost::asio::awaitable<int> render_goods_detail_content(size_t connection_id, std::string merchant, std::string goods_id, boost::beast::tcp_stream& client, int httpver, bool keepalive);
		boost::asio::awaitable<void> do_ws_read(size_t connection_id, client_connection_ptr);
		boost::asio::awaitable<void> do_ws_write(size_t connection_id, client_connection_ptr);

		boost::asio::awaitable<void> close_all_ws();

		boost::asio::awaitable<void> websocket_write(client_connection& connection_, std::string message);

		boost::asio::awaitable<boost::json::object> handle_jsonrpc_call(client_connection_ptr, const std::string& method, boost::json::object params);

		boost::asio::awaitable<boost::json::object> handle_jsonrpc_user_api(client_connection_ptr, const req_method method, boost::json::object params);
		boost::asio::awaitable<boost::json::object> handle_jsonrpc_order_api(client_connection_ptr, const req_method method, boost::json::object params);
		boost::asio::awaitable<boost::json::object> handle_jsonrpc_cart_api(client_connection_ptr, const req_method method, boost::json::object params);

		boost::asio::awaitable<void> send_notify_message(std::uint64_t uid_, const std::string&, std::int64_t exclude_connection);

		// round robing for acceptor.
		auto& get_executor(){ return m_io_context_pool.get_io_context(); }

		client_connection_ptr accept_new_connection(boost::beast::tcp_stream&& tcp_stream, std::int64_t connection_id, std::string remote_address);
		boost::asio::awaitable<void> cleanup_connection(client_connection_ptr);

	private:
		io_context_pool& m_io_context_pool;
		boost::asio::io_context& m_io_context;

		server_config m_config;
		cmall_database m_database;

		boost::asio::thread_pool git_operation_thread_pool;

		services::persist_session session_cache_map;
		services::verifycode telephone_verifier;
		services::payment payment_service;

		std::map<std::uint64_t, std::shared_ptr<services::repo_products>> merchant_repos;

		// ws 服务端相关.
		std::shared_mutex active_users_mtx;
		active_session_map active_users;
		std::vector<httpd::acceptor<client_connection_ptr, cmall_service>> m_ws_acceptors;
		friend class httpd::acceptor<client_connection_ptr, cmall_service>;
		std::atomic_bool m_abort{false};
	};
}
