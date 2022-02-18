
#include "cmall/cmall.hpp"
#include "db.hpp"
#include "logging.hpp"
#include "cmall/magic_enum.hpp"
#include "misc.hpp"
#include "cmall/internal.hpp"
#include "cmall/simple_http.hpp"
#include "session.hpp"

#include <boost/date_time.hpp>
#include <boost/json/src.hpp>

#include <boost/regex.hpp>

#include <tuple>
#include <variant>
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

namespace cmall
{
	using namespace std::chrono_literals;

	const auto build_response = overloaded{
		[](http::status code, const std::string reason = {})
		{
			response res{ code, 11 };
			res.set(http::field::server, HTTPD_VERSION_STRING);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(false);
			res.body() = reason.empty() ? std::string(http::obsolete_reason(code)) : reason;
			res.prepare_payload();
			return res;
		},
		[](boost::json::value data)
		{
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

	const auto rpc_result_to_string = [](const request_context& ctx, const rpc_result& ret) -> std::string
	{
		const auto& [success, res] = ret;

		boost::json::object obj = {
			{ "jsonrpc", "2.0" },
		};
		if (ctx.payload_.as_object().contains("id"))
		{
			obj.emplace("id", ctx.payload_.as_object().at("id"));
		}
		boost::json::string_view key = success ? "result" : "error"; // error value MUST be a object
		obj.emplace(key, res);
		return json_to_string(obj, false);
	};

	cmall_service::cmall_service(io_context_pool& ios, const server_config& config)
		: m_io_context_pool(ios)
		, m_config(config)
		, m_database(m_config.dbcfg_, m_io_context_pool.database_io_context())
	{
	}

	cmall_service::~cmall_service() { LOG_DBG << "~cmall_service()"; }

	void cmall_service::start()
	{
		m_abort = false;

		init_http_acceptors();

		int pool_size = static_cast<int>(m_io_context_pool.pool_size());
		for (int i = 0; i < pool_size; i++)
		{
			for (auto& a : m_http_acceptors)
			{
				boost::asio::spawn(m_io_context_pool.get_io_context().get_executor(),
					[this, &a](boost::asio::yield_context yield) mutable { start_http_listen(a, yield); });
			}
		}

		// for test
		boost::asio::spawn(m_io_context_pool.get_io_context().get_executor(),
			[this](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;

				auto executor = boost::asio::get_associated_executor(yield);
				boost::asio::steady_timer timer(executor);

				// cmall_merchant m;
				// m.uid_ = 10668;
				// m.name_ = "test merchant";
				// m.verified_ = true;
				// m.desc_ = "this is a test merchant";
				// bool ok = m_database.async_add(m, yield[ec]);
				// LOG_DBG << "add merchant: " << ok << ", id: " << m.id_;

				// ok = m_database.async_hard_remove<cmall_merchant>(2, yield[ec]);
				// LOG_DBG << "hard remove 2: " << ok;

				cmall_product p;
				p.owner_	   = 1000;
				p.name_		   = "keyboard";
				p.price_	   = cpp_numeric("880");
				p.currency_	   = "cny";
				p.description_ = "hhhhhh";
				p.state_	   = 1;
				bool ok		   = m_database.async_add(p, yield[ec]);
				LOG_DBG << "add product: " << ok << ", id: " << p.id_;
				// m_database.async_soft_remove(p, yield[ec]);

				timer.expires_after(5s);
				timer.async_wait(yield[ec]);

				p.state_ = 7;
				ok		 = m_database.async_update(p, yield[ec]);
				LOG_DBG << "update product: " << ok << ", id: " << p.id_;
			});
	}

	void cmall_service::stop()
	{
		m_abort = true;

		boost::system::error_code ec;

		for (auto& acceptor : m_http_acceptors)
		{
			acceptor.cancel(ec);
			acceptor.close(ec);
		}

		LOG_DBG << "database shutdown...";
		m_database.shutdown();

		LOG_DBG << "cmall_service.stop()";
	}

	bool cmall_service::init_http_acceptors()
	{
		boost::system::error_code ec;

		for (const auto& wsd : m_config.http_listens_)
		{
			tcp::endpoint endp;

			bool ipv6only = make_listen_endpoint(wsd, endp, ec);
			if (ec)
			{
				LOG_ERR << "http server listen error: " << wsd << ", ec: " << ec.message();
				return false;
			}

			tcp::acceptor a{ m_io_context_pool.get_io_context() };

			a.open(endp.protocol(), ec);
			if (ec)
			{
				LOG_ERR << "http server open accept error: " << ec.message();
				return false;
			}

			a.set_option(boost::asio::socket_base::reuse_address(true), ec);
			if (ec)
			{
				LOG_ERR << "http server accept set option failed: " << ec.message();
				return false;
			}

#if __linux__
			if (ipv6only)
			{
				int on = 1;
				if (::setsockopt(a.native_handle(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&on, sizeof(on)) == -1)
				{
					LOG_ERR << "http server setsockopt IPV6_V6ONLY";
					return false;
				}
			}
#else
			boost::ignore_unused(ipv6only);
#endif
			a.bind(endp, ec);
			if (ec)
			{
				LOG_ERR << "http server bind failed: " << ec.message() << ", address: " << endp.address().to_string()
						<< ", port: " << endp.port();
				return false;
			}

			a.listen(boost::asio::socket_base::max_listen_connections, ec);
			if (ec)
			{
				LOG_ERR << "http server listen failed: " << ec.message();
				return false;
			}

			m_http_acceptors.emplace_back(std::move(a));
		}

		return true;
	}

	void cmall_service::start_http_listen(tcp::acceptor& a, boost::asio::yield_context& yield)
	{
		boost::system::error_code error;

		while (!m_abort)
		{
			tcp::socket socket(m_io_context_pool.get_io_context());
			a.async_accept(socket, yield[error]);
			if (error)
			{
				LOG_ERR << "http server, async_accept: " << error.message();

				if (error == boost::asio::error::operation_aborted || error == boost::asio::error::bad_descriptor)
				{
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
			size_t cid = id++;

			boost::asio::spawn(
				m_io_context_pool.get_io_context().get_executor(),
				[this, cid, stream = std::move(stream)](boost::asio::yield_context yield) mutable
				{ start_http_connect(cid, std::move(stream), yield); },
				boost::coroutines::attributes(5 * 1024 * 1024));
		}

		LOG_DBG << "start_http_listen exit...";
	}

	void cmall_service::start_http_connect(
		size_t cid, boost::beast::tcp_stream stream, boost::asio::yield_context& yield)
	{
		boost::system::error_code ec;

		auto do_respond = [&](http::status status, const std::string& msg)
		{
			auto res = build_response(status, msg);
			boost::beast::http::serializer<false, string_body, fields> sr{ res };
			boost::beast::http::async_write(stream, sr, yield[ec]);
			if (ec)
			{
				LOG_WARN << "start_http_connect, " << cid << ", err: " << ec.message();
				return;
			}
		};

		for (; !m_abort;)
		{
			boost::beast::flat_buffer buffer;
			request req;
			bool keep_alive = false;

			boost::beast::http::async_read(stream, buffer, req, yield[ec]);
			if (ec)
			{
				LOG_DBG << "start_http_connect, " << cid << ", async_read: " << ec.message();
				break;
			}

			std::string target = req.target().to_string();
			if (target.empty() || target[0] != '/' || target.find("..") != beast::string_view::npos)
			{
				do_respond(http::status::bad_request, "bad request");
				break;
			}

			keep_alive = req.keep_alive();

			if (target != "/wsapi")
			{
				// websocket path error
				do_respond(http::status::bad_request, "bad request target");
				break;
			}

			if (!beast::websocket::is_upgrade(req))
			{
				do_respond(http::status::upgrade_required, "upgrade required");
				break;
			}

			std::string remote_host;
			auto endp = stream.socket().remote_endpoint(ec);
			if (!ec)
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

			stream.expires_never();

			websocket::stream<beast::tcp_stream> ws{ std::move(stream) };
			ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
			ws.async_accept(req, yield[ec]);
			if (ec)
			{
				LOG_WARN << "start_http_connect, " << cid << ", ws async_accept: " << ec.what();
				break;
			}

			auto s = std::make_shared<session>(cid, std::move(ws));
			s->register_hander([this](request_context ctx) { this->handle_request(ctx); });
			s->start();

			{
				std::lock_guard<std::mutex> lck(m_mtx_session);
				m_sessions.emplace(cid, s);
			}

			return;
		}

		if (stream.socket().is_open())
			stream.socket().shutdown(tcp::socket::shutdown_send, ec); // ?? async_accept
	}

	void cmall_service::handle_request(const request_context& ctx)
	{
		auto methodstr = json_as_string(json_accessor(ctx.payload_.as_object()).get("method", ""));
		auto method	   = magic_enum::enum_cast<req_methods>(methodstr);
		if (!method.has_value())
		{
			// unknown method
			return;
		}
		std::variant<std::int64_t, std::string, std::nullptr_t> id;
		const auto& reqo = ctx.payload_.as_object();

		auto is_request = reqo.contains("id");
		// 		if (!reqo.contains("id")) {
		// 			id = nullptr;
		// 		} else {
		// 			auto f_id = reqo.at("id");
		// 			if (f_id.is_int64()) {
		// 				id = f_id.as_int64();
		// 			} else if (f_id.is_string()) {
		// 				id = json_as_string(f_id);
		// 			}
		// 		}

		rpc_result ret;
		auto method_enum = method.value();
		switch (method_enum)
		{
			case req_methods::user_login:
			{
				ret = on_user_login(ctx);
			}
			break;
			case req_methods::user_list_product:
				break;
			case req_methods::user_list_order:
				break;
			case req_methods::user_place_order:
				break;
			case req_methods::user_get_order:
				break;
			case req_methods::user_pay_order:
				break;
			case req_methods::user_close_order:
				break;
			case req_methods::merchant_info:
				break;
			case req_methods::merchant_list_product:
				break;
			case req_methods::merchant_add_product:
				break;
			case req_methods::merchant_mod_product:
				break;
			case req_methods::merchant_del_product:
				break;
			default:
			{
				// not possible
			}
		}

		if (is_request)
		{
			auto result = rpc_result_to_string(ctx, ret);
			{
				std::lock_guard<std::mutex> lck(m_mtx_session);
				auto it = m_sessions.find(ctx.sid_);
				if (it != m_sessions.end())
				{
					auto s = it->second;
					s->reply(result);
				}
			}
		}
	}

	rpc_result cmall_service::on_user_login(const request_context& ctx)
	{
		bool success = false;

		boost::json::value ret;
		const auto& reqo = ctx.payload_.as_object();
		do
		{
			if (!reqo.contains("params"))
			{
				success = false;
				ret		= { { "code", 4 }, { "message", "params required" } };
				break;
			}

			const auto& params = reqo.at("params").as_object();
			if (!params.contains("username") || !params.contains("password"))
			{
				success = false;
				ret		= { { "code", 4 }, { "message", "username/password required" } };
				break;
			}
			auto username = json_as_string(json_accessor(params).get("username", ""));
			auto password = json_as_string(json_accessor(params).get("password", ""));
			if (username.empty() || password.size() < 6)
			{
				success = false;
				ret		= { { "code", 4 }, { "message", "username/password invalid input" } };
				break;
			}

			bool check_ok = true;
			// check username/password combination
			if (!check_ok)
			{
				success = false;
				ret		= { { "code", 1001 }, { "message", "username/password not match" } };
				break;
			}

			success = true;
			ret		= { { "uid", 9527 }, { "username", "9527" } };
		} while (false);

		return std::tie(success, ret);
	}

	void cmall_service::on_version(const http_params& params)
	{
		boost::system::error_code ec;
		response res{ boost::beast::http::status::ok, params.request_.version() };
		res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
		res.set(boost::beast::http::field::content_type, "text/html");
		res.keep_alive(params.request_.keep_alive());
		res.body() = cmall_VERSION_MIME;
		res.prepare_payload();

		boost::beast::http::serializer<false, string_body, fields> sr{ res };
		boost::beast::http::async_write(params.stream_, sr, params.yield_[ec]);
		if (ec)
		{
			LOG_WARN << "on_version, " << params.connection_id_ << ", err: " << ec.message();
			return;
		}
	}

	void cmall_service::on_get_record(const http_params& params) { boost::system::error_code ec; }
	void cmall_service::on_add_record(const http_params& params) { boost::system::error_code ec; }
	void cmall_service::on_mod_record(const http_params& params) { boost::system::error_code ec; }
	void cmall_service::on_del_record(const http_params& params)
	{
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
