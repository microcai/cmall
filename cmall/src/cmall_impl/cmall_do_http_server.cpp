
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"
#include "cmall/conversion.hpp"
#include "cmall/http_static_file_handle.hpp"

#include "httpd/http_misc_helper.hpp"
#include "httpd/header_helper.hpp"
#include "httpd/httpd.hpp"

template <typename Iterator>
auto make_iterator_range(std::pair<Iterator, Iterator> pair)
{
	return boost::make_iterator_range(pair.first, pair.second);
}

auto is_browser(auto user_agent, auto sec_websocket_protocol)
{
	if (user_agent.empty())
		return false;
	if (sec_websocket_protocol== "request_cookie")
	{
		if (user_agent.find("Mozilla") != std::string::npos)
		{
			return true;
		}
	}
	if (user_agent.find("Mozilla/5.0 (") == 0)
		return true;
	return false;
//	 user_agent.length() && (user_agent.find("Mozilla")!= std::string::npos) && sec_websocket_protocol== "request_cookie"
}

auto is_microapp(auto user_agent, auto sec_websocket_protocol)
{
	if (user_agent.empty())
		return false;
	if (sec_websocket_protocol== "request_cookie")
	{
		if (user_agent.find("Mozilla") != std::string::npos)
		{
			return true;
		}
	}
	if (user_agent.find("ToutiaoMicroApp") != std::string::npos)
		return true;
	if (user_agent.find("wechat") != std::string::npos)
		return true;
	return false;
//	 user_agent.length() && (user_agent.find("Mozilla")!= std::string::npos) && sec_websocket_protocol== "request_cookie"
}

namespace cmall
{
	awaitable<void> cmall_service::close_all_ws()
	{
		co_await httpd::detail::map(m_ws_acceptors,
			[](auto&& a) mutable -> awaitable<void> { co_return co_await a.clean_shutdown(); });

		co_await httpd::detail::map(m_wss_acceptors,
			[](auto&& a) mutable -> awaitable<void> { co_return co_await a.clean_shutdown(); });

		co_await httpd::detail::map(m_ws_unix_acceptors,
			[](auto&& a) mutable -> awaitable<void> { co_return co_await a.clean_shutdown(); });

		LOG_DBG << "cmall_service::close_all_ws() success!";
	}


	client_connection_ptr cmall_service::make_shared_connection(
		const boost::asio::any_io_executor& io, std::int64_t connection_id)
	{
		if constexpr (httpd::has_so_reuseport())
			return std::make_shared<client_connection>(io, connection_id);
		else
			return std::make_shared<client_connection>(m_io_context_pool.get_io_context(), connection_id);
	}

	client_connection_ptr cmall_service::make_shared_ssl_connection(const boost::asio::any_io_executor& io, std::int64_t connection_id)
	{
		if constexpr (httpd::has_so_reuseport())
			return std::make_shared<client_connection>(io, sslctx_, connection_id);
		else
			return std::make_shared<client_connection>(m_io_context_pool.get_io_context(), sslctx_, connection_id);
	}

	client_connection_ptr cmall_service::make_shared_unixsocket_connection(const boost::asio::any_io_executor&, std::int64_t connection_id)
	{
		return std::make_shared<client_connection>(m_io_context_pool.get_io_context(), connection_id, 0);
	}

	awaitable<void> cmall_service::client_connected(client_connection_ptr client_ptr)
	{
		using string_body = boost::beast::http::string_body;
		using fields	  = boost::beast::http::fields;
		using request	  = boost::beast::http::request<string_body>;
		using response	  = boost::beast::http::response<string_body>;
		using header_field = boost::beast::http::field;

		const size_t connection_id = client_ptr->connection_id_;

		bool keep_alive = false;

		auto http_simple_error_page
			= [&client_ptr](auto body, auto status_code, unsigned version) mutable -> awaitable<void>
		{
			response res{ static_cast<boost::beast::http::status>(status_code), version };
			res.set(header_field::server, HTTPD_VERSION_STRING);
			res.set(header_field::content_type, "text/html");
			res.keep_alive(false);
			res.body() = body;
			res.prepare_payload();

			boost::beast::http::serializer<false, string_body, fields> sr{ res };
			co_await boost::beast::http::async_write(client_ptr->tcp_stream, sr, use_awaitable);
			client_ptr->tcp_stream.close();
		};

		LOG_FMT("coro created: handle_accepted_client({})", connection_id);

		do
		{
			boost::beast::flat_buffer buffer;
			boost::beast::http::request_parser<boost::beast::http::string_body> parser_;

			parser_.body_limit(2000);

			co_await boost::beast::http::async_read(client_ptr->tcp_stream, buffer, parser_, use_awaitable);
			request req = parser_.release();
			keep_alive = req.keep_alive();

			// 这里是为了能提取到客户端的 IP 地址，即便服务本身运行在 nginx 的后面。
			auto x_real_ip = req["x-real-ip"];
			auto x_forwarded_for = req["x-forwarded-for"];
			if (x_real_ip.empty())
			{
				client_ptr->x_real_ip = client_ptr->remote_host_;
			}
			else
			{
				client_ptr->x_real_ip = x_real_ip;
			}

			if (!x_forwarded_for.empty())
			{
				std::vector<std::string> x_forwarded_clients;
				boost::split(x_forwarded_clients, x_forwarded_for, boost::is_any_of(", "));
				client_ptr->x_real_ip = x_forwarded_clients[0];
			}

			std::string_view target = req.target();

			LOG_FMT("coro: handle_accepted_client: [{}], got request on {}", connection_id, target);

			if (target.empty() || target[0] != '/' || target.find("..") != boost::beast::string_view::npos)
			{
				co_await http_simple_error_page(
					"Illegal request-target", boost::beast::http::status::bad_request, req.version());
			}

			if (!client_ptr->session_info)
			{
				// 处理 cookie.
				for (auto& cookie : make_iterator_range(req.equal_range(header_field::cookie)))
				{
					auto sessionid = httpd::parse_cookie(cookie.value())["Session"];

					if ((!sessionid.empty()) && (co_await session_cache_map.exist(sessionid)) )
					{
						// 从 cookie 里直接 recover_session
						client_ptr->session_info
							= std::make_shared<services::client_session>(co_await session_cache_map.load(sessionid));

						if (client_ptr->session_info->user_info)
						{
							co_await load_user_info(client_ptr);
						}
						co_await session_cache_map.update_lifetime(sessionid, client_ptr->x_real_ip);
						break;
					}
				}
			}

			// 处理 HTTP 请求.
			if (boost::beast::websocket::is_upgrade(req))
			{
				LOG_FMT("ws client incoming: [{}], remote: {}, real_remote: {}", connection_id, client_ptr->remote_host_, client_ptr->x_real_ip);

				if (!target.starts_with("/api"))
				{
					co_await http_simple_error_page(
						"not allowed", boost::beast::http::status::forbidden, req.version());
					break;
				}

				client_ptr->ws_client.emplace(client_ptr->tcp_stream);

				std::string cookie_line;
				std::string_view user_agent = req[header_field::user_agent];
				auto sec_websocket_protocol = req[header_field::sec_websocket_protocol];

				if (is_browser(user_agent, sec_websocket_protocol))
				{
					if (!client_ptr->session_info)
					{
						co_await alloca_sessionid(client_ptr);

						cookie_line = std::format("Session={}; Path=/api; Expires={}",
							client_ptr->session_info->session_id,
							httpd::make_http_last_modified(std::time(NULL) + 31536000)
						);
					}
				}
				else if (is_microapp(user_agent, sec_websocket_protocol))
				{
					if (!client_ptr->session_info)
					{
						co_await alloca_sessionid(client_ptr);

						cookie_line = std::format("Session={}; Path=/api; Expires={}",
							client_ptr->session_info->session_id,
							httpd::make_http_last_modified(std::time(NULL) + 31536000)
						);
					}
				}
				else
				{
					LOG_WARN << "Non broswer accessed cmall with UA:" << user_agent;
				}

				boost::beast::websocket::permessage_deflate permessage_deflate_opt;
				permessage_deflate_opt.client_enable = true; // for clients
				permessage_deflate_opt.server_enable = true; // for servers
				client_ptr->ws_client->ws_stream_.set_option(permessage_deflate_opt);

				auto timeout_opt = boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server);
				timeout_opt.idle_timeout = std::chrono::seconds(45);
				client_ptr->ws_client->ws_stream_.set_option(timeout_opt);

				client_ptr->ws_client->ws_stream_.set_option(
					boost::beast::websocket::stream_base::decorator([&sec_websocket_protocol, &cookie_line](auto& res)
					{
						res.set(header_field::server, HTTPD_VERSION_STRING);
						if (!sec_websocket_protocol.empty())
							res.set(header_field::sec_websocket_protocol, sec_websocket_protocol);
						if (!cookie_line.empty())
							res.set(boost::beast::http::field::set_cookie, cookie_line);
					}));

				co_await client_ptr->ws_client->ws_stream_.async_accept(req, use_awaitable);

				// 获取executor.
				auto executor = client_ptr->tcp_stream.get_executor();

				client_ptr->ws_client->m_disable_ping = (req["x-tencent-ua"] == "Qcloud");

				// 接收到pong, 重置超时定时器.
				client_ptr->ws_client->ws_stream_.control_callback(
					[&tcp_stream = client_ptr->tcp_stream](
						boost::beast::websocket::frame_type ft, boost::beast::string_view)
					{
						if (ft == boost::beast::websocket::frame_type::pong)
							tcp_stream.expires_after(std::chrono::seconds(60));
					});

				using namespace boost::asio::experimental::awaitable_operators;

				co_await (
					// 启动读写协程.
					do_ws_read(connection_id, client_ptr) || do_ws_write(connection_id, client_ptr));
				LOG_FMT("ws connection [{}] exit", connection_id);
				co_return;
			}
			else
			{
				boost::match_results<std::string_view::const_iterator> w;
				if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/((images|css)/.+)")))
				{
					std::string merchant = w[1].str();
					std::string remains = httpd::decodeURIComponent(w[2].str());

					int status_code = co_await render_git_repo_files(
						connection_id, merchant, remains, client_ptr->tcp_stream, req);

					if ((status_code != 200) && (status_code != 206))
					{
						co_await http_simple_error_page("ERRORED", status_code, req.version());
					}
				}
				else if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/callback\\.js([?/](.+))?")))
				{
					std::string merchant = w[1].str();
					std::string remains;
					if (w.size() == 5)
						remains = httpd::decodeURIComponent(w[4].str());

					auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);
					boost::system::error_code ec;
					auto merchant_repo_ptr = get_merchant_git_repo(merchant_id, ec);

					if (ec)
					{
						co_await http_simple_error_page("ERRORED", 404, req.version());
						continue;
					}

					std::string callback_js = co_await merchant_repo_ptr->get_file_content("scripts/callback.js", ec);
					if (ec)
					{
						co_await http_simple_error_page("ERRORED", 404, req.version());
						continue;
					}

					std::map<std::string, std::string> script_env;

					for (auto& kv : req)
					{
						script_env.insert({std::string(kv.name_string()), std::string(kv.value())});
					}

					std::string template_api_token = co_await gen_temp_api_token(merchant);
					script_env.insert({"_METHOD", std::string(req.method_string())});
					script_env.insert({"_PATH", remains});
					script_env.insert({"_API_TOKEN", template_api_token});
					if (client_ptr->session_info && client_ptr->session_info->user_info)
					{
						std::string visitor_uid = fmt::format("{}", client_ptr->session_info->user_info->uid_);
						script_env.insert({"_VISITOR", visitor_uid});
					}

					// 然后运行 callback.js
					std::string response_body = co_await script_runner.run_script(callback_js,
						req.body(), script_env, {});

					co_await drop_temp_api_token(template_api_token);

					std::map<boost::beast::http::field, std::string> headers;

					headers.insert({boost::beast::http::field::expires, httpd::make_http_last_modified(std::time(0) + 60)});
					headers.insert({boost::beast::http::field::content_type, "text/plain"});

					ec = co_await httpd::send_string_response_body(client_ptr->tcp_stream,
						response_body,
						std::move(headers),
						req.version(),
						keep_alive);
					if (ec)
						throw boost::system::system_error(ec);

				}
				else if (
					boost::regex_match(target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/scripts/([^\\.]+\\.js)([?/](.+))?"))
					||
					boost::regex_match(target.begin(), target.end(), w, boost::regex("/api/cgi-scripts/([0-9]+)/([^\\.]+\\.js)([?/](.+))?"))
				)
				{
					std::string merchant = w[1].str();
					std::string script_name = w[2].str();
					std::string remains;
					if (w.size() == 5)
						remains = httpd::decodeURIComponent(w[4].str());

					auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);
					boost::system::error_code ec;
					auto merchant_repo_ptr = get_merchant_git_repo(merchant_id, ec);

					if (ec)
					{
						co_await http_simple_error_page("ERRORED", 404, req.version());
						continue;
					}

					std::string callback_js = co_await merchant_repo_ptr->get_file_content("scripts/" + script_name, ec);
					if (ec)
					{
						co_await http_simple_error_page("ERRORED", 404, req.version());
						continue;
					}

					std::map<std::string, std::string> script_env;

					for (auto& kv : req)
					{
						script_env.insert({std::string(kv.name_string()), std::string(kv.value())});
					}

					std::string template_api_token = co_await gen_temp_api_token(merchant);

					script_env.insert({"_METHOD", std::string(req.method_string())});
					script_env.insert({"_PATH", remains});
					script_env.insert({"_API_TOKEN", template_api_token});
					if (client_ptr->session_info && client_ptr->session_info->user_info)
					{
						std::string visitor_uid = fmt::format("{}", client_ptr->session_info->user_info->uid_);
						script_env.insert({"_VISITOR", visitor_uid});
					}

					LOG_DBG << "runing script: " << script_name;

					// 然后运行 script_name
					std::string response_body = co_await script_runner.run_script(callback_js,
						req.body(), script_env, {});

					co_await drop_temp_api_token(template_api_token);

					std::map<boost::beast::http::field, std::string> headers;

					headers.insert({boost::beast::http::field::expires, httpd::make_http_last_modified(std::time(0) + 60)});
					headers.insert({boost::beast::http::field::content_type, "text/plain"});

					ec = co_await httpd::send_string_response_body(client_ptr->tcp_stream,
						response_body,
						std::move(headers),
						req.version(),
						keep_alive);
					if (ec)
						throw boost::system::system_error(ec);

				}
				else if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/(pages/.+)")))
				{
					std::string merchant = w[1].str();
					std::string remains = httpd::decodeURIComponent(w[2].str());

					if (remains.find_first_of('?') != std::string::npos)
					{
						remains = remains.substr(0, remains.find_first_of('?'));
					}

					int status_code = co_await render_git_repo_files(
						connection_id, merchant, remains, client_ptr->tcp_stream, req);

					if (status_code == 404)
					{
						status_code = co_await render_git_repo_files(connection_id, merchant, "pages/index.html", client_ptr->tcp_stream, req);
					}

					if ((status_code != 200) && (status_code != 206))
					{
						co_await http_simple_error_page("ERRORED", status_code, req.version());
					}
				}
				else if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/pages/")))
				{
					std::string merchant = w[1].str();

					int status_code = co_await render_git_repo_files(
						connection_id, merchant, "pages/index.html", client_ptr->tcp_stream, req);

					if ((status_code != 200) && (status_code != 206))
					{
						co_await http_simple_error_page("ERRORED", status_code, req.version());
					}
				}
				// 这个 /goods/${merchant}/${goods_id} 获取 富文本的商品描述.
				else if (target.starts_with("/goods"))
				{
					boost::match_results<std::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/goods/([^/]+)/([^/]+)")))
					{
						std::string merchant = w[1].str();
						std::string goods_id = httpd::decodeURIComponent(w[2].str());

						int status_code = co_await render_goods_detail_content(merchant, goods_id, client_ptr->tcp_stream, req);

						if (status_code != 200)
						{
							co_await http_simple_error_page("ERRORED", status_code, req.version());
							continue;
						}
					}
					else
					{
						co_await http_simple_error_page("access denied", 401, req.version());
					}
					continue;
				}
				else if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/api/wx/pay\\.action")))
				{
					bool callback_handled = false;
					for (auto wxpay_service : wxpay_services)
					{
						services::weixin::notify_message nofity_msg;
						try
						{
							nofity_msg = co_await wxpay_service.second->decode_notify_message(req.body(), req["Wechatpay-Timestamp"], req["Wechatpay-Nonce"], req["Wechatpay-Signature"]);
						}catch(std::exception& e)
						{
							// if e == nofity_message_decode_failed
							continue;
						}

						// TODO, 根据 notify_msg 的内容处理支付结果.
						bool success = co_await handle_order_wx_callback(nofity_msg);
						if (success)
						{
							co_await http_simple_error_page(R"json(SUCCESS)json", 200, req.version());
							callback_handled = true;
							break;
						}
					}
					if (!callback_handled)
						co_await http_simple_error_page(R"json({"code": "FAIL", "message": "失败"})json", 500, req.version());
					continue;
				}


				// 这里使用 zip 里打包的 angular 页面. 对不存在的地址其实直接替代性的返回 index.html 因此此
				// api 绝对不返回 400. 如果解压内部 zip 发生错误, 会放回 500 错误代码.
				int status_code = co_await http_handle_static_file(connection_id, req, client_ptr->tcp_stream);

				if (status_code != 200)
				{
					co_await http_simple_error_page("ERRORED", 404, req.version());
					keep_alive &= !req.need_eof();
					continue;
				}

				keep_alive &= !req.need_eof();
			}
		} while (keep_alive);

		if (!keep_alive)
		{
			co_await client_ptr->tcp_stream.async_teardown(boost::beast::role_type::server, use_awaitable);
		}

		LOG_DBG << "handle_accepted_client: HTTP connection closed : " << connection_id;
	}

	awaitable<void> cmall_service::client_disconnected(client_connection_ptr c)
	{
		std::unique_lock<std::shared_mutex> l(active_users);
		active_users.get<1>().erase(c->connection_id_);
		co_return;
	}
}
