
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/scope_exit.hpp>

#include "boost/asio/use_awaitable.hpp"
#include "cmall/cmall.hpp"
#include "cmall/db.hpp"
#include "utils/async_connect.hpp"
#include "utils/scoped_exit.hpp"
#include "utils/url_parser.hpp"

#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>

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

#include "cmall/http_static_file_handle.hpp"
#include "cmall/js_util.hpp"
#include "jsonrpc.hpp"
#include "magic_enum.hpp"

namespace cmall
{
	using namespace std::chrono_literals;

	//////////////////////////////////////////////////////////////////////////

	cmall_service::cmall_service(io_context_pool& ios, const server_config& config)
		: m_io_context_pool(ios)
		, m_io_context(m_io_context_pool.server_io_context())
		, m_config(config)
		, m_database(m_config.dbcfg_, m_io_context_pool.database_io_context())
		, session_cache_map(m_config.session_cache_file)
		, telephone_verifier(m_io_context)
	{
	}

	cmall_service::~cmall_service() { LOG_DBG << "~cmall_service()"; }

	boost::asio::awaitable<void> cmall_service::stop()
	{
		m_abort = true;

		LOG_DBG << "close all ws...";
		co_await close_all_ws();

		LOG_DBG << "database shutdown...";
		m_database.shutdown();

		LOG_DBG << "cmall_service.stop()";
	}

	boost::asio::awaitable<int> cmall_service::run_httpd()
	{
		// 初始化ws acceptors.
		co_await init_ws_acceptors();

		constexpr int concurrent_accepter = 20;

		try
		{
			std::vector<boost::asio::experimental::promise<void(std::exception_ptr)>> ws_runners;
			for (auto& a : m_ws_acceptors)
			{
				for (int i = 0; i < concurrent_accepter; i++)
				{
					ws_runners.emplace_back(boost::asio::co_spawn(a.get_executor(),
						listen_loop(a), boost::asio::experimental::use_promise));
				}
			}

			for (auto&& ws_runner : ws_runners)
				co_await ws_runner.async_wait(boost::asio::use_awaitable);
		}
		catch (boost::system::system_error& e)
		{
			LOG_ERR << "run_httpd errored: " << e.code().message();
			throw;
		}
		co_return 0;
	}

	boost::asio::awaitable<bool> cmall_service::init_ws_acceptors()
	{
		boost::system::error_code ec;

		for (const auto& wsd : m_config.ws_listens_)
		{
			tcp::endpoint endp;

			bool ipv6only = make_listen_endpoint(wsd, endp, ec);
			if (ec)
			{
				LOG_ERR << "WS server listen error: " << wsd << ", ec: " << ec.message();
				co_return false;
			}

			tcp::acceptor a{ m_io_context };

			a.open(endp.protocol(), ec);
			if (ec)
			{
				LOG_ERR << "WS server open accept error: " << ec.message();
				co_return false;
			}

			a.set_option(boost::asio::socket_base::reuse_address(true), ec);
			if (ec)
			{
				LOG_ERR << "WS server accept set option failed: " << ec.message();
				co_return false;
			}

#if __linux__
			if (ipv6only)
			{
				int on = 1;
				if (::setsockopt(a.native_handle(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&on, sizeof(on)) == -1)
				{
					LOG_ERR << "WS server setsockopt IPV6_V6ONLY";
					co_return false;
				}
			}
#else
			boost::ignore_unused(ipv6only);
#endif
			a.bind(endp, ec);
			if (ec)
			{
				LOG_ERR << "WS server bind failed: " << ec.message() << ", address: " << endp.address().to_string()
						<< ", port: " << endp.port();
				co_return false;
			}

			a.listen(boost::asio::socket_base::max_listen_connections, ec);
			if (ec)
			{
				LOG_ERR << "WS server listen failed: " << ec.message();
				co_return false;
			}

			m_ws_acceptors.emplace_back(std::move(a));
		}

		co_return true;
	}

	boost::asio::awaitable<void> cmall_service::listen_loop(tcp::acceptor& a)
	{
		while (!m_abort)
		{

			boost::system::error_code error;
			tcp::socket socket(m_io_context_pool.get_io_context());
			co_await a.async_accept(socket, boost::asio::redirect_error(boost::asio::use_awaitable, error));

			if (error)
			{
				LOG_ERR << "WS server, async_accept: " << error.message();

				if (error == boost::asio::error::operation_aborted || error == boost::asio::error::bad_descriptor)
				{
					co_return;
				}

				if (!a.is_open())
					co_return;
				continue;
			}

			boost::asio::socket_base::keep_alive option(true);
			socket.set_option(option, error);

			boost::beast::tcp_stream stream(std::move(socket));

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

			auto client_ptr = add_ws(connection_id, remote_host, std::move(stream));

			boost::asio::co_spawn(socket.get_executor(), handle_accepted_client(connection_id, client_ptr),
				[this, connection_id, client_ptr](std::exception_ptr)
				{
					remove_ws(connection_id);
					LOG_DBG << "coro exit: handle_accepted_client( " << connection_id << ")";
				});
		}

		LOG_DBG << "httpd accept loop exit...";
	}

	boost::asio::awaitable<void> cmall_service::handle_accepted_client(
		size_t connection_id, client_connection_ptr client_ptr)
	{
		using string_body = boost::beast::http::string_body;
		using fields	  = boost::beast::http::fields;
		using request	  = boost::beast::http::request<string_body>;
		using response	  = boost::beast::http::response<string_body>;

		bool keep_alive = false;

		auto http_simple_error_page
			= [&client_ptr](auto body, auto status_code, unsigned version) mutable -> boost::asio::awaitable<void>
		{
			response res{ static_cast<boost::beast::http::status>(status_code), version };
			res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
			res.set(boost::beast::http::field::content_type, "text/html");
			res.keep_alive(false);
			res.body() = body;
			res.prepare_payload();

			boost::beast::http::serializer<false, string_body, fields> sr{ res };
			co_await boost::beast::http::async_write(client_ptr->tcp_stream, sr, boost::asio::use_awaitable);
			client_ptr->tcp_stream.close();
		};

		LOG_DBG << "coro created: handle_accepted_client( " << connection_id << ")";

		do
		{
			boost::beast::flat_buffer buffer;
			request req;

			co_await boost::beast::http::async_read(client_ptr->tcp_stream, buffer, req, boost::asio::use_awaitable);
			boost::string_view target = req.target();

			LOG_DBG << "coro: handle_accepted_client: [" << connection_id << "], got request on " << target.to_string();

			if (target.empty() || target[0] != '/' || target.find("..") != boost::beast::string_view::npos)
			{
				co_await http_simple_error_page(
					"Illegal request-target", boost::beast::http::status::bad_request, req.version());
			}
			// 处理 HTTP 请求.
			else if (boost::beast::websocket::is_upgrade(req))
			{
				LOG_DBG << "ws client incoming: " << connection_id << ", remote: " << client_ptr->remote_host_;

				if (!target.starts_with("/api"))
				{
					co_await http_simple_error_page(
						"not allowed", boost::beast::http::status::forbidden, req.version());
					break;
				}

				client_ptr->ws_client.emplace(client_ptr->tcp_stream);

				client_ptr->ws_client->ws_stream_.set_option(boost::beast::websocket::stream_base::decorator(
					[](auto& res) { res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING); }));

				co_await client_ptr->ws_client->ws_stream_.async_accept(req, boost::asio::use_awaitable);

				// 获取executor.
				auto executor = client_ptr->tcp_stream.get_executor();

				// 接收到pong, 重置超时定时器.
				client_ptr->ws_client->ws_stream_.control_callback(
					[&tcp_stream = client_ptr->tcp_stream](
						boost::beast::websocket::frame_type ft, boost::beast::string_view)
					{
						if (ft == boost::beast::websocket::frame_type::pong)
							tcp_stream.expires_after(std::chrono::seconds(60));
					});

				using namespace boost::asio::experimental::awaitable_operators;

				co_await(
					// 启动读写协程.
					do_ws_read(connection_id, client_ptr) && do_ws_write(connection_id, client_ptr));
				LOG_DBG << "handle_accepted_client, " << connection_id << ", connection exit.";
			}
			else
			{
				if (target.starts_with("/api"))
				{
					co_await http_simple_error_page(
						"not allowed", boost::beast::http::status::forbidden, req.version());
					continue;
				}

				// 这个 /goods/${merchant}/${goods_id} 获取 富文本的商品描述.
				if (target.starts_with("/goods"))
				{
					boost::match_results<boost::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/goods/([^/]+)/([^/]+)")))
					{
						std::string merhcant = w[1].str();
						std::string goods_id = w[2].str();

						int status_code = co_await render_goods_detail_content(
							connection_id, merhcant, goods_id, client_ptr->tcp_stream, req.version());

						if (status_code != 200)
						{
							co_await http_simple_error_page("ERRORED", status_code, req.version());
						}
					}
					else
					{
						co_await http_simple_error_page("access denied", 401, req.version());
					}
					continue;
				}

				// 这里使用 zip 里打包的 angular 页面. 对不存在的地址其实直接替代性的返回 index.html 因此此
				// api 绝对不返回 400. 如果解压内部 zip 发生错误, 会放回 500 错误代码.
				int status_code = co_await http_handle_static_file(connection_id, req, client_ptr->tcp_stream);

				if (status_code != 200)
				{
					co_await http_simple_error_page("internal server error", status_code, req.version());
				}

				keep_alive = !req.need_eof();
			}
		} while (keep_alive && (!m_abort));
	}

	// 成功给用户返回内容, 返回 200. 如果没找到商品, 不要向 client 写任何数据, 直接放回 404, 由调用方统一返回错误页面.
	boost::asio::awaitable<int> cmall_service::render_goods_detail_content(size_t connection_id, std::string merchant,
		std::string goods_id, boost::beast::tcp_stream& client, int http_ver)
	{
		// TODO

		co_return 404;
	}

	boost::asio::awaitable<void> cmall_service::do_ws_read(size_t connection_id, client_connection_ptr connection_ptr)
	{
		auto& ws = connection_ptr->ws_client->ws_stream_;

		boost::beast::error_code ec;

		while (!m_abort)
		{
			boost::beast::multi_buffer buffer{ 4 * 1024 * 1024 }; // max multi_buffer size 4M.
			co_await ws.async_read(buffer, boost::asio::use_awaitable);

			auto body = boost::beast::buffers_to_string(buffer.data());
			auto jv	  = boost::json::parse(body, ec, {}, { 64, false, false, true });
			if (ec)
			{
				LOG_ERR << "do_ws_read, parse json error: " << ec.message() << ", body: " << body;
				co_return;
			}

			if (!jv.is_object())
			{
				co_return;
			}

			auto method = jsutil::json_as_string(jsutil::json_accessor(jv).get("method", ""));

			boost::asio::co_spawn(
				connection_ptr->m_io,
				[this, connection_ptr, method, jv]() -> boost::asio::awaitable<void>
				{
					boost::json::object replay_message;
					try
					{
						replay_message = co_await on_client_invoke(connection_ptr, method, jv);
					}
					catch (boost::system::system_error& e)
					{
						replay_message["error"] = { "code", e.code().value(), "message", e.code().message() };
					}
					catch (std::exception& e)
					{
						replay_message["error"] = { "code", 502, "message", "internal server error" };
					}
					replay_message.insert_or_assign("id", jv.at("id"));
					co_await websocket_write(connection_ptr, json_to_string(replay_message));
				},
				boost::asio::detached);
		}
	}

	boost::asio::awaitable<boost::json::object> cmall_service::on_client_invoke(
		client_connection_ptr connection_ptr, const std::string& methodstr, boost::json::value jv)
	{
		client_connection& this_client = * connection_ptr;
		boost::json::object reply_message;

		boost::system::error_code ec;

		auto method = magic_enum::enum_cast<req_method>(methodstr);
		if (!method.has_value())
		{
			throw boost::system::system_error(boost::system::error_code(cmall::error::unknown_method));
		}

		if (!this_client.session_info && (method.value() != req_method::recover_session))
		{
			throw boost::system::system_error(boost::system::error_code(cmall::error::session_needed));
		}

		switch (method.value())
		{
			case req_method::recover_session:
			{
				std::string sessionid = jsutil::json_as_string(jsutil::json_accessor(jv).get("sessionid", ""));

				if (sessionid.empty() || !(co_await session_cache_map.exist(sessionid)))
				{
					// TODO 表示是第一次使用，所以创建一个新　session 给客户端.
				}

				this_client.session_info = std::make_shared<services::client_session>(co_await session_cache_map.load(sessionid));
				if (this_client.session_info->user_info)
				{
					co_await m_database.async_load<cmall_user>(this_client.session_info->user_info->db_user_entry.uid_,
						this_client.session_info->user_info->db_user_entry);
				}

				reply_message["result"] = true;

			}break;

			case req_method::user_prelogin:
			{
				services::verify_session verify_session_cookie;
				// 拿到 verify_code_token,  要和 verify_code 一并传给 user.login
				// verify_code_token 的有效期为 2 分钟.
				// verify_code_token 对同一个手机号只能一分钟内请求一次.

				std::string tel = jsutil::json_as_string(jsutil::json_accessor(jv).get("telephone", ""));
				// TODO 获取验证码.
				verify_session_cookie = co_await telephone_verifier.send_verify_code(tel, ec);

				if (ec)
				{
					// TODO, 汇报验证码发送失败.
				}
				else
				{
					this_client.session_info->verify_session_cookie = verify_session_cookie;
					reply_message["result"] = true;
				}

			}break;
			case req_method::user_login:
			{
				std::string verify_code = jsutil::json_as_string(jsutil::json_accessor(jv).get("verify_code", ""));

				if (this_client.session_info->verify_session_cookie)
				{
					if (co_await telephone_verifier.verify_verify_code(verify_code, this_client.session_info->verify_session_cookie.value()))
					{
						// SUCCESS.
						cmall_user user;
						if (co_await m_database.async_load_user_by_phone(
								this_client.session_info->verify_telephone, user))
						{


						}
					}
				}
				else
				{

				}


				// TODO 认证成功后， sessionid 写入 mdbx 数据库以便日后恢复.
			}break;

			case req_method::user_logout: break;
			case req_method::user_list_products: break;
			case req_method::user_apply_merchant: break;
			case req_method::cart_add: break;
			case req_method::cart_del: break;
			case req_method::cart_list: break;
			case req_method::order_create_cart: break;
			case req_method::order_create_direct: break;
			case req_method::order_status: break;
			case req_method::order_close: break;

			case req_method::goods_list:
			{
				// 列出 商品, 根据参数决定是首页还是商户
				auto merchant = jsutil::json_as_string(jsutil::json_accessor(jsutil::json_accessor(jv).get("params", boost::json::object{})).get("merchant", ""));

				if (merchant == "")
				{
					// 列举首页商品.

					// 首页商品由管理员设置

					// TODO
					// m_database.async_load_front_page_goods();

					// 格式化.

					//
				}
				else
				{
					// 列出商户的上架商品.
				}
			}break;
			case req_method::goods_detail:
			{
				// 获取商品信息, 注意这个不是商品描述, 而是商品 标题, 价格, 和缩略图. 用在商品列表页面.

				// TODO
				// NOTE: 商品的描述, 使用 GET 操作, 以便这种大段的富文本描述信息能被 CDN 缓存．

			}break;

			case req_method::merchant_product_add: break;
			case req_method::merchant_product_mod: break;
			case req_method::merchant_product_launch: break;
			case req_method::merchant_product_withdraw: break;
			case req_method::admin_user_list: break;
			case req_method::admin_user_ban: break;
			case req_method::admin_merchant_list: break;
			case req_method::admin_merchant_ban: break;
			case req_method::admin_product_list: break;
			case req_method::admin_product_withdraw: break;
			case req_method::admin_order_force_refund: break;
		}

		co_return reply_message;
	}

	boost::asio::awaitable<void> cmall_service::do_ws_write(size_t connection_id, client_connection_ptr connection_ptr)
	{
		auto& message_deque = connection_ptr->ws_client->message_channel;
		auto& ws			= connection_ptr->ws_client->ws_stream_;

		using namespace boost::asio::experimental::awaitable_operators;

		while (!m_abort)
			try
			{
				timer t(co_await boost::asio::this_coro::executor);
				t.expires_from_now(std::chrono::seconds(15));
				std::variant<std::monostate, std::string> awaited_result
					= co_await(t.async_wait(boost::asio::use_awaitable)
						|| message_deque.async_receive(boost::asio::use_awaitable));
				if (awaited_result.index() == 0)
				{
					LOG_DBG << "coro: do_ws_write: [" << connection_id << "], send ping to client";
					co_await ws.async_ping("", boost::asio::use_awaitable); // timed out
				}
				else
				{
					auto message = std::get<1>(awaited_result);
					if (message.empty())
						co_return;
					co_await ws.async_write(boost::asio::buffer(message), boost::asio::use_awaitable);
				}
			}
			catch (boost::system::system_error& e)
			{
				boost::system::error_code ec = e.code();
				connection_ptr->tcp_stream.close();
				LOG_ERR << "coro: do_ws_write: [" << connection_id << "], exception:" << ec.message();
				co_return;
			}
	}

	client_connection_ptr cmall_service::add_ws(
		size_t connection_id, const std::string& remote_host, boost::beast::tcp_stream&& tcp_stream)
	{
		std::lock_guard<std::mutex> lock(m_ws_mux);
		client_connection_ptr connection_ptr
			= std::make_shared<client_connection>(std::move(tcp_stream), connection_id, remote_host);
		m_ws_streams.insert(std::make_pair(connection_id, connection_ptr));
		return connection_ptr;
	}

	void cmall_service::remove_ws(size_t connection_id)
	{
		std::lock_guard<std::mutex> lock(m_ws_mux);
		m_ws_streams.erase(connection_id);
	}

	boost::asio::awaitable<void> cmall_service::close_all_ws()
	{
		{
			std::lock_guard<std::mutex> lock(m_ws_mux);
			for (auto& ws : m_ws_streams)
			{
				auto& conn_ptr = ws.second;
				conn_ptr->close();
			}
		}

		while (m_ws_streams.size())
		{
			timer t(m_io_context.get_executor());
			t.expires_from_now(std::chrono::milliseconds(20));
			co_await t.async_wait(boost::asio::use_awaitable);
		}
	}

	boost::asio::awaitable<void> cmall_service::websocket_write(
		client_connection_ptr connection_ptr, std::string message)
	{
		if (connection_ptr->ws_client)
			boost::asio::post(connection_ptr->tcp_stream.get_executor(),
				[connection_ptr, message = std::move(message)]() mutable
				{ connection_ptr->ws_client->message_channel.try_send(boost::system::error_code(), message); });
		co_return;
	}

	boost::asio::awaitable<void> cmall_service::notify_message_to_all_client(std::string message)
	{
		std::lock_guard<std::mutex> lock(m_ws_mux);
		for (auto& ws : m_ws_streams)
		{
			auto& conn_ptr = ws.second;
			if (!conn_ptr)
				continue;
			co_await websocket_write(conn_ptr, message);
		}
	}

	boost::asio::awaitable<void> cmall_service::mitigate_chaindb() { co_return; }

	boost::asio::awaitable<std::shared_ptr<ws_stream>> cmall_service::connect(size_t index)
	{
		std::shared_ptr<ws_stream> wsp = std::make_shared<ws_stream>(co_await boost::asio::this_coro::executor);
		tcp::socket& sock			   = boost::beast::get_lowest_layer(*wsp).socket();

		tcp::resolver resolver{ m_io_context };

		if (m_config.upstreams_.empty())
			co_return std::shared_ptr<ws_stream>{};

		if (index >= m_config.upstreams_.size())
			co_return std::shared_ptr<ws_stream>{};

		// 连接上游服务器.
		for (; index < m_config.upstreams_.size(); index++)
		{
			auto& upstream = m_config.upstreams_[index];
			util::uri parser;

			if (!parser.parse(upstream))
				co_return std::shared_ptr<ws_stream>{};

			auto const results = co_await resolver.async_resolve(
				std::string(parser.host()), std::string(parser.port()), boost::asio::use_awaitable);
			co_await asio_util::async_connect(sock, results, boost::asio::use_awaitable);

			std::string origin = "all";
			auto decorator	   = [origin](boost::beast::websocket::request_type& m)
			{ m.insert(boost::beast::http::field::origin, origin); };

			wsp->set_option(boost::beast::websocket::stream_base::decorator(decorator));
			co_await wsp->async_handshake(
				std::string(parser.host()), parser.path().empty() ? "/" : parser.path(), boost::asio::use_awaitable);
			if (true)
				break;
		}

		co_return wsp;
	}

}
