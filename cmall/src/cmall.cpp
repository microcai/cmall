#include "boost/algorithm/string/join.hpp"
#include "boost/asio/buffer.hpp"
#include "boost/asio/ip/address_v4.hpp"
#include "boost/asio/ip/basic_endpoint.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/asio/socket_base.hpp"
#include "boost/asio/spawn.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/serializer.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/http/verb.hpp"
#include "boost/beast/http/write.hpp"
#include "boost/concept_check.hpp"
#include "boost/date_time/posix_time/time_formatters.hpp"
#include "boost/json/detail/array.hpp"
#include "boost/json/parse.hpp"
#include "boost/json/serialize.hpp"
#include "boost/json/value.hpp"
#include "boost/system/detail/error_code.hpp"
#include "db.hpp"
#include "logging.hpp"
#include "cmall/async_connect.hpp"
#include "cmall/scoped_exit.hpp"
#include "cmall/simple_http.hpp"
#include "cmall/url_parser.hpp"
#include "cmall/cmall.hpp"

#include "cmall/io.hpp"

#include <algorithm>
#include <boost/date_time.hpp>
#include <boost/json.hpp>

#include <boost/regex.hpp>

#include <cctype>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <version.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#endif

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/printf.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace cmall {
	using namespace std::chrono_literals;

	const auto build_response = overloaded{
		[](http::status code, const std::string reason = {}) {
			response res{ code, 11 };
			res.set(http::field::server, HTTPD_VERSION_STRING);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(false);
			res.body() = reason.empty() ? std::string(http::obsolete_reason(code)) : reason;
			res.prepare_payload();
			return res;
		},
		[](boost::json::value data) {
			std::string payload = boost::json::serialize(data);
			response res{ http::status::ok, 11 };
			res.set(http::field::server, HTTPD_VERSION_STRING);
			res.set(http::field::content_type, "text/html");
			res.set(http::field::content_type, "application/json");
			res.keep_alive(false);
			res.body() = payload;
			res.prepare_payload();
			return res;
		},
	};

	cmall_service::cmall_service(io_context_pool& ios, const server_config& config)
		: m_io_context_pool(ios)
		, m_config(config)
		, m_database(m_config.dbcfg_, m_io_context_pool.database_io_context()) { }

	cmall_service::~cmall_service() { LOG_DBG << "~cmall_service()"; }

	void cmall_service::start() {
		m_abort = false;

		init_http_acceptors();

		int pool_size = static_cast<int>(m_io_context_pool.pool_size());
		for (int i = 0; i < pool_size; i++) {
			for (auto& a : m_http_acceptors) {
				boost::asio::spawn(m_io_context_pool.get_io_context().get_executor(),
					[this, &a](boost::asio::yield_context yield) mutable { start_http_listen(a, yield); });
			}
		}

		// for test
		boost::asio::spawn(m_io_context_pool.get_io_context().get_executor(),
			[this](boost::asio::yield_context yield) mutable {
				boost::system::error_code ec;

				cmall_merchant m;
				m.uid_ = 10668;
				m.name_ = "test merchant";
				m.verified_ = true;
				m.desc_ = "this is a test merchant";
				bool ok = m_database.async_add(m, yield[ec]);
				LOG_DBG << "add merchant: " << ok << ", id: " << m.id_;


				ok = m_database.async_hard_remove<cmall_merchant>(2, yield[ec]);
				LOG_DBG << "hard remove 2: " << ok;

				cmall_product p;
				p.owner_ = 1000;
				p.name_ = "keyboard";
				p.price_ = cpp_numeric("880");
				p.currency_ = "cny";
				p.description_ = "hhhhhh";
				p.state_ = 1;
				ok = m_database.async_add(p, yield[ec]);
				LOG_DBG << "add product: " << ok << ", id: " << p.id_;
				// m_database.async_soft_remove(p, yield[ec]);

				p.state_ = 2;
				m_database.async_update(p, yield[ec]);
			}
		);
	}

	void cmall_service::stop() {
		m_abort = true;

		boost::system::error_code ec;
		if (m_sock4->is_open()) {
			m_sock4->cancel(ec);
			m_sock4->close(ec);
		}
		if (m_sock6->is_open()) {
			m_sock6->cancel(ec);
			m_sock6->close(ec);
		}

		for (auto& acceptor : m_http_acceptors) {
			acceptor.cancel(ec);
			acceptor.close(ec);
		}

		LOG_DBG << "database shutdown...";
		m_database.shutdown();

		LOG_DBG << "cmall_service.stop()";
	}

	bool cmall_service::init_http_acceptors() {
		boost::system::error_code ec;

		for (const auto& wsd : m_config.http_listens_) {
			tcp::endpoint endp;

			bool ipv6only = make_listen_endpoint(wsd, endp, ec);
			if (ec) {
				LOG_ERR << "http server listen error: " << wsd << ", ec: " << ec.message();
				return false;
			}

			tcp::acceptor a{ m_io_context_pool.get_io_context() };

			a.open(endp.protocol(), ec);
			if (ec) {
				LOG_ERR << "http server open accept error: " << ec.message();
				return false;
			}

			a.set_option(boost::asio::socket_base::reuse_address(true), ec);
			if (ec) {
				LOG_ERR << "http server accept set option failed: " << ec.message();
				return false;
			}

#if __linux__
			if (ipv6only) {
				int on = 1;
				if (::setsockopt(a.native_handle(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&on, sizeof(on)) == -1) {
					LOG_ERR << "http server setsockopt IPV6_V6ONLY";
					return false;
				}
			}
#else
			boost::ignore_unused(ipv6only);
#endif
			a.bind(endp, ec);
			if (ec) {
				LOG_ERR << "http server bind failed: " << ec.message() << ", address: " << endp.address().to_string()
						<< ", port: " << endp.port();
				return false;
			}

			a.listen(boost::asio::socket_base::max_listen_connections, ec);
			if (ec) {
				LOG_ERR << "http server listen failed: " << ec.message();
				return false;
			}

			m_http_acceptors.emplace_back(std::move(a));
		}

		return true;
	}

	void cmall_service::start_http_listen(tcp::acceptor& a, boost::asio::yield_context& yield) {
		boost::system::error_code error;

		while (!m_abort) {
			tcp::socket socket(m_io_context_pool.get_io_context());
			a.async_accept(socket, yield[error]);
			if (error) {
				LOG_ERR << "http server, async_accept: " << error.message();

				if (error == boost::asio::error::operation_aborted || error == boost::asio::error::bad_descriptor) {
					return;
				}

				if (!a.is_open())
					return;

				continue;
			}

			boost::asio::socket_base::keep_alive option(true);
			socket.set_option(option, error);

			boost::beast::tcp_stream stream(std::move(socket));

			static std::atomic_size_t id{ 0 };
			size_t connection_id = id++;

			boost::asio::spawn(
				m_io_context_pool.get_io_context().get_executor(),
				[this, connection_id, stream = std::move(stream)](boost::asio::yield_context yield) mutable {
					start_http_connect(connection_id, std::move(stream), yield);
				},
				boost::coroutines::attributes(5 * 1024 * 1024));
		}

		LOG_DBG << "start_http_listen exit...";
	}

	void cmall_service::start_http_connect(
		size_t connection_id, boost::beast::tcp_stream stream, boost::asio::yield_context& yield) {
		boost::system::error_code ec;

		for (; !m_abort;) {
			boost::beast::flat_buffer buffer;
			request req;
			bool keep_alive = false;

			boost::beast::http::async_read(stream, buffer, req, yield[ec]);
			if (ec) {
				LOG_DBG << "start_http_connect, " << connection_id << ", async_read: " << ec.message();
				break;
			}

			std::string target = req.target().to_string();
			if (target.empty() || target[0] != '/' || target.find("..") != beast::string_view::npos) {
				auto res = build_response(http::status::bad_request, "Illegal request-target");

				boost::beast::http::serializer<false, string_body, fields> sr{ res };
				boost::beast::http::async_write(stream, sr, yield[ec]);
				if (ec) {
					LOG_WARN << "start_http_connect, " << connection_id << ", err: " << ec.message();
					return;
				}

				break;
			}

			keep_alive = req.keep_alive();
			if (!beast::websocket::is_upgrade(req)) {
				boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(60));

				http_params params{ {}, connection_id, stream, req, yield };
				boost::smatch what;

#define BEGIN_HTTP_EVENT()                                                                                             \
	if (false) { }
#define ON_HTTP_EVENT(exp, func)                                                                                       \
	else if (boost::regex_match(target, what, boost::regex{ exp })) {                                                  \
		for (auto i = 1; i < static_cast<int>(what.size()); i++)                                                       \
			params.command_.emplace_back(what[i]);                                                                     \
		func(params);                                                                                                  \
	}
#define END_HTTP_EVENT()                                                                                               \
	else {                                                                                                             \
		auto res = build_response(http::status::not_found);                                                            \
		http::serializer<false, string_body, fields> sr{ res };                                                        \
		http::async_write(params.stream_, sr, params.yield_[ec]);                                                      \
	}

				BEGIN_HTTP_EVENT()
				ON_HTTP_EVENT("^/record(([/\?]{1}).*)?$", on_record_op)
				ON_HTTP_EVENT("^/version$", on_version)
				END_HTTP_EVENT()

				if (!keep_alive)
					break;
				continue;
			}

			// if (target != "/wsrpc") {
			// 	if (!keep_alive)
			// 		break;
			// 	continue;
			// }

			std::string remote_host;
			auto endp = stream.socket().remote_endpoint(ec);
			if (!ec) {
				if (endp.address().is_v6()) {
					remote_host = "[" + endp.address().to_string() + "]:" + std::to_string(endp.port());
				} else {
					remote_host = endp.address().to_string() + ":" + std::to_string(endp.port());
				}
			}

			stream.expires_never();

			return;
		}

		if (stream.socket().is_open())
			stream.socket().shutdown(tcp::socket::shutdown_send, ec); // ?? async_accept
	}

	void cmall_service::on_record_op(const http_params& params) {

		boost::system::error_code ec;

		auto& req = params.request_;

		// TODO: auth

		switch (req.method()) {
			case http::verb::get: // get
				on_get_record(params);
				break;
			case http::verb::post: // add
				on_add_record(params);
				break;
			case http::verb::patch: // modify
				on_mod_record(params);
				break;
			case http::verb::delete_: // remove
				on_del_record(params);
				break;
			default: {
				auto res = build_response(http::status::method_not_allowed);
				http::serializer<false, string_body, fields> sr{ res };
				http::async_write(params.stream_, sr, params.yield_[ec]);
				return;
			}
		}
	}

	void cmall_service::on_version(const http_params& params) {
		boost::system::error_code ec;
		response res{ boost::beast::http::status::ok, params.request_.version() };
		res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
		res.set(boost::beast::http::field::content_type, "text/html");
		res.keep_alive(params.request_.keep_alive());
		res.body() = cmall_VERSION_MIME;
		res.prepare_payload();

		boost::beast::http::serializer<false, string_body, fields> sr{ res };
		boost::beast::http::async_write(params.stream_, sr, params.yield_[ec]);
		if (ec) {
			LOG_WARN << "on_version, " << params.connection_id_ << ", err: " << ec.message();
			return;
		}
	}

	void cmall_service::on_get_record(const http_params& params) {
		boost::system::error_code ec;
	}
	void cmall_service::on_add_record(const http_params& params) {
		boost::system::error_code ec;
	}
	void cmall_service::on_mod_record(const http_params& params) {
		boost::system::error_code ec;
	}
	void cmall_service::on_del_record(const http_params& params) {
		boost::system::error_code ec;
		// response res;
		// do {
		// 	auto& req		 = params.request_;
		// 	std::string body = req.body();
		// 	LOG_INFO << "del record: " << body;

		// 	boost::json::object payload;
		// 	boost::json::value req_json = boost::json::parse(body, ec);
		// 	if (ec) {
		// 		LOG_WARN << "del record bad request: " << body;
		// 		res = build_response(http::status::bad_request);
		// 		break;
		// 	}
		// 	if (!req_json.is_object()) {
		// 		LOG_WARN << "del record bad request: " << body;
		// 		res = build_response(http::status::bad_request);
		// 		break;
		// 	}
		// 	payload = req_json.as_object();

		// 	if (!payload.if_contains("id") || !payload["id"].is_int64()) {
		// 		res = build_response(http::status::bad_request);
		// 		break;
		// 	}
		// 	auto id = payload["id"].as_int64();
		// 	if (id < 0) {
		// 		res = build_response(http::status::bad_request);
		// 		break;
		// 	}

		// 	m_database.async_remove_record(id, params.yield_[ec]);

		// 	boost::json::value data = {
		// 		{ "code", 0 },
		// 	};
		// 	res = build_response(data);
		// } while (false);

		// http::serializer<false, string_body, fields> sr{ res };
		// http::async_write(params.stream_, sr, params.yield_[ec]);
		// if (ec) {
		// 	LOG_WARN << "on_del_record error: " << ec.message();
		// }
	}
}
