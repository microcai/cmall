
#pragma once

#include <boost/variant2.hpp>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <unordered_map>

#include "coroyield.hpp"
#include "default_cert.hpp"
#include "uawaitable.hpp"
#include "url_parser.hpp"

#include <iostream>

using boost::asio::awaitable;

namespace httpc
{
	namespace net	= boost::asio;
	namespace beast = boost::beast;
	namespace http	= beast::http;

	struct request_options_t
	{
		boost::asio::io_context& ioc;
		http::verb verb;
		std::string url;
		std::unordered_map<std::string, std::string> headers;
		std::optional<std::string> body;
	};
	struct response_t
	{
		std::optional<std::string> err;
		int code;
		std::string content_type;
		std::string body;
	};

	namespace detail
	{
		using namespace boost::variant2;
		using http_stream  = beast::tcp_stream;
		using https_stream = beast::ssl_stream<http_stream>;
		using stream_type  = variant<monostate, http_stream, https_stream>;

		inline awaitable<response_t> do_request(request_options_t&& option)
		{
			using namespace std::chrono_literals;
			boost::system::error_code ec;
			auto& ioc = option.ioc;

			response_t ret;

			do
			{
				auto url = util::uri(option.url);
				net::ip::tcp::resolver resolver(ioc);
				auto const results
					= co_await resolver.async_resolve(url.host(), url.port(), asio_util::use_awaitable[ec]);
				if (ec)
					break;

				auto is_secure = url.scheme() == "https";
				stream_type stream;

				if (is_secure)
				{
					net::ssl::context ctx(net::ssl::context::tls_client);
					auto cert = default_root_certificates();
					ctx.add_certificate_authority(boost::asio::buffer(cert.data(), cert.size()), ec);
					ctx.set_verify_mode(net::ssl::verify_peer);
					beast::ssl_stream<beast::tcp_stream> s(ioc, ctx);
					if (!SSL_set_tlsext_host_name(s.native_handle(), url.host().data()))
					{
						ec.assign(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
						break;
					}
					stream = std::move(s);
				}
				else
				{
					stream.emplace<http_stream>(ioc);
				}

				if (!is_secure)
				{
					auto& s = get<1>(stream);
					s.expires_after(30s);
					co_await s.async_connect(results, asio_util::use_awaitable[ec]);
				}
				else
				{
					auto& s = get<2>(stream);
					beast::get_lowest_layer(s).expires_after(30s);
					co_await beast::get_lowest_layer(s).async_connect(results, asio_util::use_awaitable[ec]);
				}
				if (ec)
					break;

				if (is_secure)
				{
					auto& s = get<2>(stream);
					beast::get_lowest_layer(s).expires_after(30s);
					co_await s.async_handshake(net::ssl::stream_base::client, asio_util::use_awaitable[ec]);
				}

				auto path = url.path();
				if (path.empty())
					path = "/";
				http::request<http::string_body> req{ option.verb, path, 11 };
				req.set(http::field::host, url.host());
				req.set(http::field::user_agent, "curl/7.82.0");
				// req.set(http::field::connection, "close");
				for (auto& header : option.headers)
				{
					req.set(header.first, header.second);
				}
				if (option.body.has_value())
				{
					req.body() = option.body.value();
					req.prepare_payload();
				}
				std::cout << req << std::endl;

				if (is_secure)
				{
					auto& s = get<2>(stream);
					beast::get_lowest_layer(s).expires_after(30s);
					co_await http::async_write(s, req, asio_util::use_awaitable[ec]);
				}
				else
				{
					auto& s = get<1>(stream);
					s.expires_after(30s);
					co_await http::async_write(s, req, asio_util::use_awaitable[ec]);
				}
				if (ec)
					break;

				beast::flat_buffer b;
				http::response<http::string_body> res;
				if (is_secure)
				{
					auto& s = get<2>(stream);
					co_await http::async_read(s, b, res, asio_util::use_awaitable[ec]);
				}
				else
				{
					auto& s = get<1>(stream);
					co_await http::async_read(s, b, res, asio_util::use_awaitable[ec]);
				}
				if (ec)
					break;

				ret.code		 = static_cast<unsigned>(res.result());
				ret.content_type = res[http::field::content_type];
				ret.body		 = res.body();

				if (is_secure)
				{
					auto& s = get<2>(stream);
					beast::get_lowest_layer(s).socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);
				}
				else
				{
					auto& s = get<1>(stream);
					s.socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);
				}

			} while (false);
			if (ec)
				ret.err = ec.message();

			co_return ret;
		}
	}

	inline awaitable<response_t> request(request_options_t&& options) { return detail::do_request(std::move(options)); }
}
