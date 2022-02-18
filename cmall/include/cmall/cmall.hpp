
//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "boost/asio/spawn.hpp"
#include "boost/json/value.hpp"
#include "cmall/database.hpp"
#include "cmall/internal.hpp"
#include "cmall/session.hpp"

#include <boost/asio.hpp>

namespace cmall
{

	struct server_config
	{
		std::vector<std::string> http_listens_;

		cmall::db_config dbcfg_;
	};

	namespace net	  = boost::asio;
	using string_body = boost::beast::http::string_body;
	using fields	  = boost::beast::http::fields;
	using request	  = boost::beast::http::request<string_body>;
	using response	  = boost::beast::http::response<string_body>;

	using rpc_result = std::tuple<bool, boost::json::value>; // (true, result) or (false, error)

	enum class req_methods
	{
		user_login,

		user_list_product,
		user_list_order,
		user_place_order,
		user_get_order,
		user_pay_order,
		user_close_order,

		merchant_info,
		merchant_list_product,
		merchant_add_product,
		merchant_mod_product,
		merchant_del_product,
	};

	class cmall_service
	{
		struct http_params
		{
			std::vector<std::string> command_;
			size_t connection_id_;
			boost::beast::tcp_stream& stream_;
			request& request_;
			boost::asio::yield_context& yield_;
		};

		// c++11 noncopyable.
		cmall_service(const cmall_service&) = delete;
		cmall_service& operator=(const cmall_service&) = delete;

	public:
		cmall_service(io_context_pool& ios, const server_config& config);
		~cmall_service();

	public:
		void start();
		void stop();

	private:
		bool init_http_acceptors();
		void start_http_listen(tcp::acceptor& a, boost::asio::yield_context& yield);
		void start_http_connect(size_t cid, boost::beast::tcp_stream stream, boost::asio::yield_context& yield);

		void handle_request(const request_context& ctx);

		void on_version(const http_params& params);

	private:
		rpc_result on_user_login(const request_context& ctx);

	private:
		void on_get_record(const http_params& params);
		void on_add_record(const http_params& params);
		void on_mod_record(const http_params& params);
		void on_del_record(const http_params& params);

	private:
		io_context_pool& m_io_context_pool;

		server_config m_config;
		cmall_config m_cmall_config;
		cmall_database m_database;

		std::vector<tcp::acceptor> m_http_acceptors;

		std::atomic_bool m_abort{ false };

		std::unordered_map<std::uint64_t, session_ptr> m_sessions;
		std::mutex m_mtx_session;
	};
}
