
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"
#include "cmall/conversion.hpp"

#include "httpd/http_misc_helper.hpp"
#include "httpd/httpd.hpp"
#include "httpd/http_stream.hpp"

#include "beast/fields_alloc.hpp"

#include "bundle_file.hpp"
#include "utils/uawaitable.hpp"

template <typename Allocator>
awaitable<int> http_handle_static_file(size_t connection_id, boost::beast::http::request<boost::beast::http::string_body, Allocator>& req, httpd::http_any_stream& client)
{
	unzFile zip_file = ::unzOpen2_64("internel_bundled_exe", &exe_bundled_file_ops);
	std::shared_ptr<void> auto_cleanup((void*)zip_file, unzClose);
	unz_global_info64 unzip_global_info64;
	std::time_t if_modified_since;
	bool have_if_modified_since = httpd::parse_gmt_time_fmt(
		req[boost::beast::http::field::if_modified_since], &if_modified_since);

	std::string_view accept_encoding = req[boost::beast::http::field::accept_encoding];
	bool accept_deflate				   = false;

	if (accept_encoding.find("deflate") != boost::string_view::npos)
		accept_deflate = true;

	if (unzGetGlobalInfo64(zip_file, &unzip_global_info64) != UNZ_OK)
		co_return 502;

	std::string_view path_ = req.target();

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

	std::time_t target_file_modifined_time = httpd::dos2unixtime(target_file_info.dosDate);

	using fields = boost::beast::http::fields;

	using response = boost::beast::http::response<boost::beast::http::buffer_body>;

	response res{ boost::beast::http::status::ok, req.version() };
	res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
	res.set(boost::beast::http::field::expires, httpd::make_http_last_modified(std::time(0) + 60));
	res.set(boost::beast::http::field::last_modified, httpd::make_http_last_modified(target_file_modifined_time));
	res.set(boost::beast::http::field::content_type, httpd::get_mime_type_from_extension(std::filesystem::path(path).extension().string()));
	res.keep_alive(req.keep_alive());

	if (have_if_modified_since && (target_file_modifined_time <= if_modified_since))
	{
		// 返回 304
		res.result(boost::beast::http::status::not_modified);

		res.set(boost::beast::http::field::expires, httpd::make_http_last_modified(std::time(0) + 60));
		boost::beast::http::response_serializer<boost::beast::http::buffer_body, fields> sr{ res };
		res.body().data = nullptr;
		res.body().more = false;
		co_await boost::beast::http::async_write(client, sr, use_awaitable);
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

	co_await boost::beast::http::async_write_header(client, sr, use_awaitable);

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

		co_await boost::beast::http::async_write(client, sr, asio_util::use_awaitable[ec]);

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
		using response	  = boost::beast::http::response<string_body>;

		const size_t connection_id = client_ptr->connection_id_;

		bool keep_alive = false;

		auto http_simple_error_page
			= [&client_ptr](auto body, auto status_code, unsigned version) mutable -> awaitable<void>
		{
			response res{ static_cast<boost::beast::http::status>(status_code), version };
			res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
			res.set(boost::beast::http::field::content_type, "text/html");
			res.keep_alive(false);
			res.body() = body;
			res.prepare_payload();

			boost::beast::http::serializer<false, string_body, fields> sr{ res };
			co_await boost::beast::http::async_write(client_ptr->tcp_stream, sr, use_awaitable);
			client_ptr->tcp_stream.close();
		};

		LOG_DBG << "coro created: handle_accepted_client( " << connection_id << ")";
		char pre_allocated_buffer[6000];

		do
		{

			fields_alloc<char> alloc_{pre_allocated_buffer, sizeof pre_allocated_buffer};

			boost::beast::static_buffer<5000> buffer;
			boost::beast::http::request_parser<boost::beast::http::string_body, fields_alloc<char>> parser_(std::piecewise_construct, std::make_tuple(), std::make_tuple(alloc_));

			parser_.body_limit(2000);

			co_await boost::beast::http::async_read(client_ptr->tcp_stream, buffer, parser_, use_awaitable);
			auto req = parser_.release();
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

			LOG_DBG << "coro: handle_accepted_client: [" << connection_id << "], got request on " << target;

			if (target.empty() || target[0] != '/' || target.find("..") != boost::beast::string_view::npos)
			{
				co_await http_simple_error_page(
					"Illegal request-target", boost::beast::http::status::bad_request, req.version());
			}
			// 处理 HTTP 请求.
			else if (boost::beast::websocket::is_upgrade(req))
			{
				LOG_DBG << "ws client incoming: " << connection_id << ", remote: " << client_ptr->x_real_ip;

				if (!target.starts_with("/api"))
				{
					co_await http_simple_error_page(
						"not allowed", boost::beast::http::status::forbidden, req.version());
					break;
				}

				client_ptr->ws_client.emplace(client_ptr->tcp_stream);

				client_ptr->ws_client->ws_stream_.set_option(boost::beast::websocket::stream_base::decorator(
					[](auto& res) { res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING); }));

				co_await client_ptr->ws_client->ws_stream_.async_accept(req, use_awaitable);

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

				co_await (
					// 启动读写协程.
					do_ws_read(connection_id, client_ptr) || do_ws_write(connection_id, client_ptr));
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

				else if (target.starts_with("/repos"))
				{
					boost::match_results<std::string_view::const_iterator> w;
					if (boost::regex_match(
							target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/((images|css)/.+)")))
					{
						std::string merchant = w[1].str();
						std::string remains	 = w[2].str();

						int status_code = co_await render_git_repo_files(
							connection_id, merchant, remains, client_ptr->tcp_stream, req.version(), req.keep_alive());

						if (status_code != 200)
						{
							co_await http_simple_error_page("ERRORED", status_code, req.version());
						}
					}
					else
					{
						co_await http_simple_error_page("ERRORED", 403, req.version());
					}
					continue;
				}

				// 这个 /goods/${merchant}/${goods_id} 获取 富文本的商品描述.
				else if (target.starts_with("/goods"))
				{
					boost::match_results<std::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/goods/([^/]+)/([^/]+)")))
					{
						std::string merchant = w[1].str();
						std::string goods_id = httpd::decodeURIComponent(w[2].str());

						int status_code = co_await render_goods_detail_content(
							connection_id, merchant, goods_id, client_ptr->tcp_stream, req.version(), req.keep_alive());

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
				// 这个 /scriptcallback/${merchant}/? 的调用, 都传给 scripts/callback.js.
				else if (target.starts_with("/scriptcallback"))
				{
					boost::match_results<std::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/scriptcallback/([0-9]+)(/.+)")))
					{
						std::string merchant = w[1].str();
						std::string remains = w[2].str();

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
						script_env.insert({"_METHOD", std::string(req.method_string())});
						script_env.insert({"_PATH", remains});

						// TODO 然后运行 callback.js
						std::string response_body = co_await script_runner.run_script(callback_js,
							req.body(), script_env, {});

						ec = co_await httpd::send_string_response_body(client_ptr->tcp_stream,
							response_body,
							httpd::make_http_last_modified(std::time(0) + 60),
							"text/plain",
							req.version(),
							keep_alive);
						if (ec)
							throw boost::system::system_error(ec);
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

				keep_alive &= !req.need_eof();
			}
		} while (keep_alive && (!m_abort));

		if (m_abort)
			co_return;

		if (!keep_alive)
		{
			co_await client_ptr->tcp_stream.async_teardown(boost::beast::role_type::server, use_awaitable);
		}

		LOG_DBG << "handle_accepted_client: HTTP connection closed : " << connection_id;
	}

	awaitable<void> cmall_service::client_disconnected(client_connection_ptr c)
	{
		std::unique_lock<std::shared_mutex> l(active_users_mtx);
		active_users.get<1>().erase(c->connection_id_);
		co_return;
	}
}
