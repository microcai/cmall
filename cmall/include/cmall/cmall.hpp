//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include "boost/asio/spawn.hpp"
#include "cmall/database.hpp"
#include "cmall/internal.hpp"
#include <cstdint>
#include <vector>

#include <boost/asio.hpp>

namespace cmall {

	struct server_config {
		std::vector<std::string> http_listens_;

		std::uint16_t dns_port_;

		cmall::db_config dbcfg_;
	};

	namespace net = boost::asio;
	using string_body = boost::beast::http::string_body;
	using fields	  = boost::beast::http::fields;
	using request	  = boost::beast::http::request<string_body>;
	using response	  = boost::beast::http::response<string_body>;
	class cmall_service {
		struct http_params {
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
		void start_http_connect(
			size_t connection_id, boost::beast::tcp_stream stream, boost::asio::yield_context& yield);

		void on_record_op(const http_params& params);
		void on_version(const http_params& params);

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

		std::shared_ptr<net::ip::udp::socket> m_sock4;
		std::shared_ptr<net::ip::udp::socket> m_sock6;

		std::vector<tcp::acceptor> m_http_acceptors;

		std::atomic_bool m_abort{ false };
	};
}
