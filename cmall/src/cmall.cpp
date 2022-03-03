
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <boost/scope_exit.hpp>
#include <vector>

#include "boost/json/value_from.hpp"
#include "boost/system/detail/error_code.hpp"
#include "boost/system/system_error.hpp"
#include "cmall/error_code.hpp"
#include "utils/async_connect.hpp"
#include "utils/scoped_exit.hpp"
#include "utils/url_parser.hpp"

#include "cmall/cmall.hpp"
#include "cmall/database.hpp"
#include "cmall/db.hpp"

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
#include "magic_enum.hpp"

#include "services/repo_products.hpp"

#include "cmall/conversion.hpp"

#ifdef __linux__
# define HAVE_REUSE_PORT 1
#endif

namespace cmall
{
	using namespace std::chrono_literals;

	//////////////////////////////////////////////////////////////////////////

	cmall_service::cmall_service(io_context_pool& ios, const server_config& config)
		: m_io_context_pool(ios)
		, m_io_context(m_io_context_pool.server_io_context())
		, m_config(config)
		, m_database(m_config.dbcfg_)
		, session_cache_map(m_config.session_cache_file)
		, telephone_verifier(m_io_context)
		, payment_service(m_io_context)
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

	boost::asio::awaitable<bool> cmall_service::load_configs()
	{
		std::vector<cmall_merchant> all_merchant;
		co_await m_database.async_load_all_merchant(all_merchant);

		for (cmall_merchant& merchant : all_merchant)
		{
			if (services::repo_products::is_git_repo(merchant.repo_path))
			{
				auto repo = std::make_shared<services::repo_products>(m_io_context, merchant.repo_path);
				this->merchant_repos.emplace(merchant.uid_, repo);
			}
			else
			{
				LOG_ERR << "no bare git repos @(" << merchant.repo_path << ") for merchant <<" << merchant.name_;
			}
		}

		co_return true;
	}

	boost::asio::awaitable<int> cmall_service::run_httpd()
	{
		// auto now	 = boost::posix_time::second_clock::local_time();
		// cmall_user u =
		//{
		//	.uid_		  = 16,
		//	.name_		  = (const char *)u8"起名好难",
		//	.active_phone = "1723424",
		//	.recipients	  = {},
		//	.verified_	  = true,
		//	.state_		  = 1,
		//	.created_at_  = now,
		//	.updated_at_  = now,
		// };
		// std::vector<Recipient> r;
		// for (auto i = 0; i < 5; i++) {
		//	Recipient it;
		//	it.address = "address " + std::to_string(i);
		//	it.province = "province " + std::to_string(i);

		//	r.emplace_back(std::move(it));
		//}
		// u.recipients		 = r;

		// auto jv = boost::json::value_from(u);
		// std::string json_str = boost::json::serialize(jv);
		// LOG_INFO << "json_str: " << json_str;

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
					ws_runners.emplace_back(boost::asio::co_spawn(
						a.get_executor(), listen_loop(a), boost::asio::experimental::use_promise));
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
#ifdef HAVE_REUSE_PORT
		typedef boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port;
#endif
		for (const auto& wsd : m_config.ws_listens_)
		{
			tcp::endpoint endp;

			bool ipv6only = make_listen_endpoint(wsd, endp, ec);
			if (ec)
			{
				LOG_ERR << "WS server listen error: " << wsd << ", ec: " << ec.message();
				co_return false;
			}

#ifdef HAVE_REUSE_PORT
			for (int io_index = 0; io_index < m_io_context_pool.pool_size(); io_index++)
			{
				tcp::acceptor a{ m_io_context_pool.get_io_context(io_index) };
#else
				tcp::acceptor a{ m_io_context };
#endif
				a.open(endp.protocol(), ec);
				if (ec)
				{
					LOG_ERR << "WS server open accept error: " << ec.message();
					co_return false;
				}

#ifdef HAVE_REUSE_PORT
				a.set_option(reuse_port(true), ec);
				if (ec)
				{
					LOG_ERR << "WS server accept set option SO_REUSEPORT failed: " << ec.message();
					co_return false;
				}
#endif

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
#ifdef HAVE_REUSE_PORT
			}
#endif
		}

		co_return true;
	}

	boost::asio::awaitable<void> cmall_service::listen_loop(tcp::acceptor& a)
	{
		while (!m_abort)
		{
			boost::system::error_code error;

#ifdef HAVE_REUSE_PORT
			tcp::socket socket(a.get_executor());
#else
			tcp::socket socket(m_io_context_pool.get_io_context());
#endif
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

			boost::asio::co_spawn(socket.get_executor(),
				handle_accepted_client(connection_id, client_ptr),
				[this, connection_id, client_ptr](std::exception_ptr)
				{
					remove_ws(connection_id);
					LOG_DBG << "coro exit: handle_accepted_client( " << connection_id << "), alive connection: " << m_ws_streams.size();
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
				co_return;
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

		LOG_DBG << "handle_accepted_client: HTTP connection closed : " << connection_id;
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
		while (!m_abort)
		{
			boost::beast::multi_buffer buffer{ 4 * 1024 * 1024 }; // max multi_buffer size 4M.
			co_await connection_ptr->ws_client->ws_stream_.async_read(buffer, boost::asio::use_awaitable);

			auto body = boost::beast::buffers_to_string(buffer.data());

			boost::system::error_code ec;
			auto jv	  = boost::json::parse(body, ec, {}, { 64, false, false, true });

			if (ec || !jv.is_object())
			{
				// 这里直接　co_return, 连接会关闭. 因为用户发来的数据连 json 都不是.
				// 对这种垃圾客户端，直接暴力断开不搭理.
				co_return;
			}

			maybe_jsonrpc_request_t maybe_req = boost::json::value_to<maybe_jsonrpc_request_t>(jv);
			if (!maybe_req.has_value())
			{
				// 格式不对.
				boost::json::object reply_message;
				if (jv.as_object().contains("id"))
					reply_message["id"]		 = jv.at("id");
				reply_message["error"]	 = { { "code", -32600 }, { "message", "Invalid Request" } };
				co_await websocket_write(connection_ptr, jsutil::json_to_string(reply_message));
				continue;
			}

			auto req	= maybe_req.value();
			auto method = req.method;
			auto params = req.params;

			client_connection& this_client = *connection_ptr;

			if (!this_client.session_info)
			{
				boost::json::object replay_message;
				// 未有 session 前， 先不并发处理 request，避免 客户端恶意并发 recover_session 把程序挂掉
				replay_message = co_await handle_jsonrpc_call(connection_ptr, method, params);
				replay_message.insert_or_assign("id", jv.at("id"));
				co_await websocket_write(connection_ptr, jsutil::json_to_string(replay_message));
				continue;
			}

			// 每个请求都单开线程处理
			boost::asio::co_spawn(
				connection_ptr->m_io,
				[this, connection_ptr, method, params, jv]() -> boost::asio::awaitable<void>
				{
					boost::json::object replay_message;
					try
					{
						replay_message = co_await handle_jsonrpc_call(connection_ptr, method, params);
					}
					catch (boost::system::system_error& e)
					{
						replay_message["error"] = { { "code", e.code().value() }, { "message", e.code().message() } };
					}
					catch (std::exception& e)
					{
						LOG_ERR << e.what();
						replay_message["error"] = { { "code", 502 }, { "message", "internal server error" } };
					}
					// 每个请求都单开线程处理, 因此客户端收到的应答是乱序的,
					// 这就是 jsonrpc 里 id 字段的重要意义.
					// 将 id 字段原原本本的还回去, 客户端就可以根据 返回的 id 找到原来发的请求
					replay_message.insert_or_assign("id", jv.at("id"));
					co_await websocket_write(connection_ptr, jsutil::json_to_string(replay_message));
				},
				boost::asio::detached);
		}
	}

	boost::asio::awaitable<boost::json::object> cmall_service::handle_jsonrpc_call(
		client_connection_ptr connection_ptr, const std::string& methodstr, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;

		auto method = magic_enum::enum_cast<req_method>(methodstr);
		if (!method.has_value())
		{
			throw boost::system::system_error(boost::system::error_code(cmall::error::unknown_method));
		}

		if (!this_client.session_info && (method.value() != req_method::recover_session))
		{
			throw boost::system::system_error(boost::system::error_code(cmall::error::session_needed));
		}

		auto ensure_login
			= [&](bool check_admin = false, bool check_merchant = false) mutable -> boost::asio::awaitable<void>
		{
			if (!this_client.session_info->user_info)
				throw boost::system::system_error(error::login_required);

			auto uid = this_client.session_info->user_info->uid_;
			if (check_admin)
			{
				// TODO, 检查用户是否是 admin 才能继续操作.
			}

			if (check_merchant)
			{
				cmall_merchant merchant;
				if (!co_await m_database.async_load<cmall_merchant>(uid, merchant))
					throw boost::system::system_error(error::merchant_user_required);
			}

			co_return;
		};

		switch (method.value())
		{
			case req_method::recover_session:
			{
				std::string sessionid = jsutil::json_accessor(params).get_string("sessionid");

				if (sessionid.empty() || !(co_await session_cache_map.exist(sessionid)))
				{
					// TODO 表示是第一次使用，所以创建一个新　session 给客户端.
					this_client.session_info = std::make_shared<services::client_session>();

					this_client.session_info->session_id = gen_uuid();
					sessionid							 = this_client.session_info->session_id;

					co_await session_cache_map.save(*this_client.session_info);
				}
				else
				{
					this_client.session_info
						= std::make_shared<services::client_session>(co_await session_cache_map.load(sessionid));
				}

				if (this_client.session_info->user_info)
				{
					co_await m_database.async_load<cmall_user>(
						this_client.session_info->user_info->uid_, *(this_client.session_info->user_info));
				}

				reply_message["result"] = { { "session_id", this_client.session_info->session_id },
					{ "isLogin", static_cast<bool>(this_client.session_info->user_info) } };
			}
			break;

			case req_method::user_prelogin:
			case req_method::user_login:
			case req_method::user_islogin:
			case req_method::user_logout:
				co_return co_await handle_jsonrpc_user_api(connection_ptr, method.value(), params);
				break;
			case req_method::user_info:
			case req_method::user_apply_merchant:
			case req_method::user_list_recipient_address:
			case req_method::user_add_recipient_address:
			case req_method::user_modify_receipt_address:
			case req_method::user_erase_receipt_address:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_user_api(connection_ptr, method.value(), params);
				break;

			case req_method::cart_add:
				break;
			case req_method::cart_del:
				break;
			case req_method::cart_list:
				break;
			case req_method::order_create_cart:
			case req_method::order_create_direct:
			case req_method::order_status:
			case req_method::order_close:
			case req_method::order_list:
			case req_method::order_get_pay_url:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_order_api(connection_ptr, method.value(), params);
				break;

			case req_method::goods_list:
			{
				std::vector<services::product> all_products;

				// 列出 商品, 根据参数决定是首页还是商户
				auto merchant = jsutil::json_accessor(params).get_string("merchant");

				if (merchant == "")
				{
					for (auto& [merchant_id, merchant_repo] : merchant_repos)
					{
						std::vector<services::product> products = co_await merchant_repo->get_products();

						for (auto& p:products)
							p.owner_ = merchant_id;

						std::copy(products.begin(), products.end(), std::back_inserter(all_products));
					}

				}
				else
				{
					auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);

					std::vector<services::product> products = co_await merchant_repos[merchant_id]->get_products();
					for (auto& p:products)
						p.owner_ = merchant_id;

					std::copy(products.begin(), products.end(), std::back_inserter(all_products));
				}

				reply_message["result"] = boost::json::value_from(all_products);
			}
			break;
			case req_method::goods_detail:
			{
				// 获取商品信息, 注意这个不是商品描述, 而是商品 标题, 价格, 和缩略图. 用在商品列表页面.

				// TODO
				// NOTE: 商品的描述, 使用 GET 操作, 以便这种大段的富文本描述信息能被 CDN 缓存．
			}
			break;
			case req_method::merchant_product_list:
			break;

			case req_method::merchant_product_add:
			{
				co_await ensure_login(false, true);

//				auto uid = this_client.session_info->user_info->uid_;

			}
			break;
			case req_method::merchant_product_mod:
				break;
			case req_method::merchant_product_launch:
				break;
			case req_method::merchant_product_withdraw:
				break;
			case req_method::admin_user_list:
				break;
			case req_method::admin_user_ban:
				break;
			case req_method::admin_merchant_list:
				break;
			case req_method::admin_merchant_ban:
				break;
			case req_method::admin_product_list:
				break;
			case req_method::admin_product_withdraw:
				break;
			case req_method::admin_order_force_refund:
				break;
		}

		co_return reply_message;
	}

	boost::asio::awaitable<boost::json::object> cmall_service::handle_jsonrpc_user_api(
		client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;
		services::client_session& session_info = *this_client.session_info;

		switch (method)
		{
			case req_method::user_prelogin:
			{
				services::verify_session verify_session_cookie;
				// 拿到 verify_code_token,  要和 verify_code 一并传给 user.login
				// verify_code_token 的有效期为 2 分钟.
				// verify_code_token 对同一个手机号只能一分钟内请求一次.

				std::string tel = jsutil::json_as_string(jsutil::json_accessor(params).get("telephone", ""));

				session_info.verify_telephone = tel;
				verify_session_cookie		  = co_await telephone_verifier.send_verify_code(tel);

				session_info.verify_session_cookie = verify_session_cookie;
				reply_message["result"]			   = true;
			}
			break;
			case req_method::user_login:
			{
				std::string verify_code = jsutil::json_accessor(params).get_string("verify_code");

				if (session_info.verify_session_cookie)
				{
					if (co_await telephone_verifier.verify_verify_code(
							verify_code, this_client.session_info->verify_session_cookie.value()))
					{
						// SUCCESS.
						cmall_user user;
						if (co_await m_database.async_load_user_by_phone(session_info.verify_telephone, user))
						{
							session_info.user_info = user;
						}
						else
						{
							cmall_user new_user;
							new_user.active_phone = this_client.session_info->verify_telephone;

							// 新用户注册，赶紧创建个新用户
							co_await m_database.async_add(new_user);
							this_client.session_info->user_info = new_user;
						}
						session_info.verify_session_cookie = {};
						// 认证成功后， sessionid 写入 mdbx 数据库以便日后恢复.
						co_await session_cache_map.save(
							this_client.session_info->session_id, *this_client.session_info);
						reply_message["result"] = { { "login", "success" }, { "usertype", "user" } };
						break;
					}
				}
				throw boost::system::system_error(cmall::error::invalid_verify_code);
			}
			break;

			case req_method::user_logout:
			{
				this_client.session_info->user_info = {};
				co_await session_cache_map.save(this_client.session_info->session_id, *this_client.session_info);
				reply_message["result"] = { "status", "success" };
			}
			break;
			case req_method::user_islogin:
			{
				reply_message["result"] = static_cast<bool>(session_info.user_info);
			}
			break;
			case req_method::user_info:
			{
				auto uid = this_client.session_info->user_info->uid_;
				cmall_user uinfo;
				co_await m_database.async_load<cmall_user>(uid, uinfo);
				reply_message["result"] = boost::json::value_from(uinfo);
			}
			break;
			case req_method::user_apply_merchant:
			{
				auto uid = session_info.user_info->uid_;
				cmall_merchant m;
				if (co_await m_database.async_load<cmall_merchant>(uid, m))
				{
					// already applied
					throw boost::system::system_error(cmall::error::already_exist);
				}

				std::string name = jsutil::json_accessor(params).get_string("name");
				if (name.empty())
					throw boost::system::system_error(cmall::error::invalid_params);

				std::string desc = jsutil::json_accessor(params).get_string("desc");

				m.uid_ = uid;
				m.name_ = name;
				m.verified_ = false;
				if (!desc.empty())
					m.desc_ = desc;
				co_await m_database.async_add<cmall_merchant>(m);
				reply_message["result"] = true;
			}
			break;
			case req_method::user_list_recipient_address:
			{
				// 重新载入 user_info, 以便获取正确的收件人地址信息.
				co_await m_database.async_load<cmall_user>(session_info.user_info->uid_, *(session_info.user_info));
				cmall_user& user_info = *(session_info.user_info);
				boost::json::array recipients_array;
				for (int i = 0; i < user_info.recipients.size(); i++)
				{
					auto jsobj =  boost::json::value_from(user_info.recipients[i]);
					jsobj.as_object()["id"] = i;
					recipients_array.push_back(jsobj);
				}
				reply_message["result"] = recipients_array;
			}
			break;
			case req_method::user_add_recipient_address:
			{
				Recipient new_address;

				// 这次这里获取不到就 throw 出去， 给客户端一点颜色 see see.
				new_address.telephone = params["telephone"].as_string();
				new_address.address	  = params["address"].as_string();
				new_address.name	  = params["name"].as_string();

				cmall_user& user_info = *(session_info.user_info);
				// 重新载入 user_info, 以便获取正确的收件人地址信息.
				bool is_db_op_ok		= co_await m_database.async_update<cmall_user>(user_info.uid_,
					   [&](cmall_user value)
					   {
						   value.recipients.push_back(new_address);
						   user_info = value;
						   return value;
					   });
				reply_message["result"] = is_db_op_ok;
			}
			break;
			case req_method::user_modify_receipt_address:
				break;
			case req_method::user_erase_receipt_address:
			{
				auto recipient_id_to_remove = params["recipient_id"].as_int64();
				cmall_user& user_info = *(session_info.user_info);

				bool is_db_op_ok = co_await m_database.async_update<cmall_user>(user_info.uid_,
					[&](cmall_user value) {
						if (recipient_id_to_remove >= 0 && recipient_id_to_remove < (int64_t)value.recipients.size())
							value.recipients.erase(value.recipients.begin() + recipient_id_to_remove);
						user_info = value;
						return value;
					});
				reply_message["result"] = is_db_op_ok;
			}
			break;
			default:
				throw "this should never be executed";
		}

		co_return reply_message;
	}

	boost::asio::awaitable<boost::json::object> cmall_service::handle_jsonrpc_order_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;
		services::client_session& session_info = *this_client.session_info;
		cmall_user& this_user = *(session_info.user_info);

		switch (method)
		{
			case req_method::order_create_cart:
			case req_method::order_create_direct:
			{
				// 重新载入 user_info, 以便获取正确的收件人地址信息.
				co_await m_database.async_load<cmall_user>(this_user.uid_, *this_client.session_info->user_info);
				cmall_user& user_info = *(this_client.session_info->user_info);

				boost::json::object goods_ref	  = jsutil::json_accessor(params).get("goods", boost::json::object{}).as_object();

				auto merchant_id_of_goods = goods_ref["merchant_id"].as_int64();
				auto goods_id_of_goods = jsutil::json_as_string(goods_ref["good_id"].as_string(), "");

				auto recipient_id = jsutil::json_accessor(params).get("recipient_id", -1).as_int64();

				if (!(recipient_id >= 0 && recipient_id < (int64_t)user_info.recipients.size()))
				{
					throw boost::system::system_error(
						boost::system::error_code(cmall::error::recipient_id_out_of_range));
				}

				services::product product_in_mall = co_await merchant_repos[merchant_id_of_goods]->get_products(goods_id_of_goods);

				goods_snapshot good_snap;

				good_snap.description_ = product_in_mall.product_description;
				good_snap.good_version_git = product_in_mall.git_version;
				good_snap.name_ = product_in_mall.product_title;
				good_snap.owner_ = merchant_id_of_goods;
				good_snap.price_ = cpp_numeric(product_in_mall.product_price);

				cmall_order new_order;

				new_order.buyer_ = user_info.uid_;
				new_order.oid_	 = gen_uuid();
				new_order.stage_ = order_unpay;
				new_order.bought_goods.push_back(good_snap);
				new_order.price_ = good_snap.price_;

				new_order.recipient.push_back(user_info.recipients[recipient_id]);

				co_await m_database.async_add(new_order);

				reply_message["result"] = {
					{ "orderid", new_order.oid_ },
				};
			}
			break;
			case req_method::order_status:
				break;
			case req_method::order_close:
				break;
			case req_method::order_list:
			{
				std::vector<cmall_order> orders;
				auto page = jsutil::json_accessor(params).get("page", 0).as_int64();
				auto page_size = jsutil::json_accessor(params).get("page_size", 20).as_int64();

				co_await m_database.async_load_all_user_orders(orders, this_user.uid_, (int)page, (int)page_size);

				LOG_DBG << "order_list retrived, " << orders.size() << " items";
				reply_message["result"] = boost::json::value_from(orders);

			}break;
			case req_method::order_get_pay_url:
			{
				auto orderid	  = jsutil::json_accessor(params).get_string("orderid");
				auto payment = jsutil::json_accessor(params).get_string("payment");

				// TODO, load order from database first,

				cmall_order order_to_pay;

				bool order_founded = co_await m_database.async_load_order(order_to_pay, orderid);

				if (!order_founded ||  (order_to_pay.buyer_ != this_user.uid_))
				{
					// TODO
					throw boost::system::system_error(boost::system::error_code(cmall::error::order_not_found));
				}

				// 对已经存在的订单, 获取支付连接.
				services::payment_url payurl = co_await payment_service.get_payurl(orderid, 0, "", to_string(order_to_pay.price_), services::PAYMENT_CHSPAY);

				// TODO, 把 payurl.valid_until 存起来， 下次用.

				reply_message["result"] = { {"type", "url"}, {"url", payurl.uri } };
			}
			break;
			default:
				throw "this should never be executed";
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
