
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/scope_exit.hpp>

#include "cmall/cmall.hpp"
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

#include "bundle_file.hpp"
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
		, mdbx_env(m_config.session_cache_file, mdbx::env::operate_parameters{})
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

		int pool_size = static_cast<int>(m_io_context_pool.pool_size());

		try
		{
			std::vector<boost::asio::experimental::promise<void(std::exception_ptr)>> ws_runners;
			for (int i = 0; i < pool_size; i++)
			{
				for (auto& a : m_ws_acceptors)
				{
					ws_runners.emplace_back(boost::asio::co_spawn(m_io_context_pool.get_io_context().get_executor(),
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
			tcp::socket socket(co_await boost::asio::this_coro::executor);
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
				int status_code = co_await do_http_handle(connection_id, req, client_ptr->tcp_stream);

				if (status_code != 200)
				{
					co_await http_simple_error_page("internal server error", status_code, req.version());
				}

				keep_alive = !req.need_eof();
			}
		} while (keep_alive && (!m_abort));
	}

	boost::asio::awaitable<int> cmall_service::do_http_handle(size_t connection_id,
		boost::beast::http::request<boost::beast::http::string_body>& req, boost::beast::tcp_stream& client)
	{
		unzFile zip_file = ::unzOpen2_64("internel_bundled_exe", &exe_bundled_file_ops);
		std::shared_ptr<void> auto_cleanup((void*)zip_file, unzClose);
		unz_global_info64 unzip_global_info64;
		std::time_t if_modified_since;
		bool have_if_modified_since = parse_gmt_time_fmt(
			req[boost::beast::http::field::if_modified_since].to_string().c_str(), &if_modified_since);

		boost::string_view accept_encoding = req[boost::beast::http::field::accept_encoding];
		bool accept_deflate				   = false;

		if (accept_encoding.find("deflate") != boost::string_view::npos)
			accept_deflate = true;

		if (unzGetGlobalInfo64(zip_file, &unzip_global_info64) != UNZ_OK)
			co_return 502;

		boost::string_view path_ = req.target();

		if (path_ == "/")
			path_ = "index.html";
		else
		{
			path_ = path_.substr(1);
		}

		std::string path{ path_.data(), path_.length() };

		if (unzLocateFile(zip_file, path.data(), false) != UNZ_OK)
		{
			path = "index.html";
			unzLocateFile(zip_file, "index.html", false);
		}

		unz_file_info64 target_file_info;

		unzGetCurrentFileInfo64(zip_file, &target_file_info, NULL, 0, NULL, 0, NULL, 0);

		std::time_t target_file_modifined_time = dos2unixtime(target_file_info.dosDate);

		using fields = boost::beast::http::fields;

		using response = boost::beast::http::response<boost::beast::http::buffer_body>;

		response res{ boost::beast::http::status::ok, req.version() };
		res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
		res.set(boost::beast::http::field::expires, make_http_last_modified(std::time(0) + 60));
		res.set(boost::beast::http::field::last_modified, make_http_last_modified(target_file_modifined_time));
		res.set(boost::beast::http::field::content_type, mime_map[boost::filesystem::path(path).extension().string()]);
		res.keep_alive(req.keep_alive());

		if (have_if_modified_since && (target_file_modifined_time <= if_modified_since))
		{
			// 返回 304
			res.result(boost::beast::http::status::not_modified);

			res.set(boost::beast::http::field::expires, make_http_last_modified(std::time(0) + 60));
			boost::beast::http::response_serializer<boost::beast::http::buffer_body, fields> sr{ res };
			res.body().data = nullptr;
			res.body().more = false;
			co_await boost::beast::http::async_write(client, sr, boost::asio::use_awaitable);
			co_return 200;
		}

		int method = 0, level = 0;

		if (unzOpenCurrentFile2(zip_file, &method, &level, accept_deflate ? 1 : 0) != UNZ_OK)
			co_return 500;

		if (method == Z_DEFLATED && accept_deflate)
		{
			res.set(boost::beast::http::field::content_encoding, "deflate");
			res.content_length(target_file_info.compressed_size);
		}
		else
		{
			res.content_length(target_file_info.uncompressed_size);
		}

		res.body().data = nullptr;
		res.body().more = true;

		boost::system::error_code ec;
		boost::beast::http::response_serializer<boost::beast::http::buffer_body, fields> sr{ res };

		co_await boost::beast::http::async_write_header(client, sr, boost::asio::use_awaitable);

		char buffer[2048];

		do
		{
			auto bytes_transferred = unzReadCurrentFile(zip_file, buffer, 2048);
			if (bytes_transferred == 0)
			{
				res.body().data = nullptr;
				res.body().more = false;
			}
			else
			{
				res.body().data = buffer;
				res.body().size = bytes_transferred;
				res.body().more = true;
			}
			co_await boost::beast::http::async_write(
				client, sr, boost::asio::redirect_error(boost::asio::use_awaitable, ec));

			if (ec == boost::beast::http::error::need_buffer)
				continue;
			if (ec)
			{
				throw boost::system::system_error(ec);
			}

		} while (!sr.is_done());

		unzCloseCurrentFile(zip_file);

		co_return 200;
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
						replay_message.insert_or_assign("error", e.what());
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
		boost::json::object reply_message;

		auto method = magic_enum::enum_cast<req_method>(methodstr);
		if (!method.has_value())
		{
			reply_message["error"] = { "code", 4, "message", "unknown method" };
			co_return reply_message;
		}
		switch (method.value()) {

		case req_method::user_login: break;
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

		if (methodstr == "goods.list")
		{
			// 列出 商品, 根据参数决定是首页还是商户
			auto merchant = jsutil::json_as_string(jsutil::json_accessor(jv).get("merchant", ""));

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
		}
		else if (methodstr == "goods.detail")
		{
			// 获取商品信息, 注意这个不是商品描述, 而是商品 标题, 价格, 和缩略图. 用在商品列表页面.

			// TODO
			// NOTE: 商品的描述, 使用 GET 操作, 以便这种大段的富文本描述信息能被 CDN 缓存．
		}
		else if (methodstr == "user.recover_session")
		{
			// TODO 从 mdbx 查找保存的 session
		}
		else if (methodstr == "user.login")
		{
			std::string verify_code = jsutil::json_as_string(jsutil::json_accessor(jv).get("verify_code", ""));
			std::string verify_code_token
				= jsutil::json_as_string(jsutil::json_accessor(jv).get("verify_code_token", ""));

			// TODO 认证成功后， sessionid 写入 mdbx 数据库以便日后恢复.
		}
		else if (methodstr == "user.prelogin")
		{
			// TODO 获取验证码.
			// 拿到 verify_code_token,  要和 verify_code 一并传给 user.login
			// verify_code_token 的有效期为 2 分钟.
			// verify_code_token 对同一个手机号只能一分钟内请求一次.
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
