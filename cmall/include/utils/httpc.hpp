
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

#include "utils/async_connect.hpp"

#include <optional>
#include <iostream>

#if __has_include("zlib.h")
#include "zlib.h"
#define HAS_ZLIB 1
#endif

using boost::asio::awaitable;

namespace httpc
{
	namespace net	= boost::asio;
	namespace beast = boost::beast;
	namespace http	= beast::http;

	struct request_options_t
	{
		http::verb verb;
		std::string url;
		std::unordered_map<std::string, std::string> headers;
		std::optional<std::string> body;
	};
	struct response_t : public http::response<http::string_body>
	{
		std::optional<std::string> err;
		int code;
		std::string_view content_type;
		std::string_view body;
	};

	namespace detail
	{
		#ifdef HAS_ZLIB
		namespace gzip
		{
			inline std::string gunzip(const char* gzip_data, size_t  gzip_data_len)
			{
				std::string deflated_data;
				// // 原始gzip格式数据
				// char gzip_data[] = {31, 139, 8, 0, 0, 0, 0, 0, 0, 0, 11, 99, 96, 28, 73, 47, 202, 47, 202, 47, 42, 214, 4, 0, 0, 0};
				// // 原始数据长度
				// size_t gzip_data_len = sizeof(gzip_data) / sizeof(char);

				// 创建zlib解压器
				z_stream stream;
				stream.zalloc = Z_NULL;
				stream.zfree = Z_NULL;
				stream.opaque = Z_NULL;
				stream.avail_in = 0;
				stream.next_in = Z_NULL;
				int ret = inflateInit2(&stream, 16 + MAX_WBITS);
				if (ret != Z_OK) {
					std::cerr << "Failed to initialize zlib: " << ret << std::endl;
					return deflated_data;
				}


				char buffer[1000];
				// 解压gzip格式数据
				stream.next_in = (Bytef*)gzip_data;
				stream.avail_in = (uInt)gzip_data_len;
				do {
					stream.next_out = (Bytef*)buffer;
					stream.avail_out = sizeof(buffer);
					ret = inflate(&stream, Z_NO_FLUSH);
					if (ret < 0) {
						std::cerr << "Failed to unzip gzip data: " << ret << std::endl;
						inflateEnd(&stream);
						return deflated_data;
					}
					deflated_data += std::string(buffer, sizeof(buffer) - stream.avail_out);
					// std::cout.write(buffer, sizeof(buffer) - stream.avail_out);
				} while (stream.avail_out == 0);

				// 关闭zlib解压器
				inflateEnd(&stream);

				return deflated_data;
			}
		}
		#endif // HAS_ZLIB

		using namespace boost::variant2;
		using http_stream  = beast::tcp_stream;
		using https_stream = beast::ssl_stream<http_stream>;
		using stream_type  = variant<monostate, http_stream, https_stream>;

		inline awaitable<response_t> do_request(request_options_t option)
		{
			using namespace std::chrono_literals;
			boost::system::error_code ec;
			auto ioc = co_await boost::asio::this_coro::executor;

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
					co_await asio_util::async_connect(s.socket(), results, asio_util::use_awaitable[ec]);
				}
				else
				{
					auto& s = get<2>(stream);
					beast::get_lowest_layer(s).expires_after(30s);
					co_await asio_util::async_connect(beast::get_lowest_layer(s).socket(), results, asio_util::use_awaitable[ec]);
				}
				if (ec)
					break;

				if (is_secure)
				{
					auto& s = get<2>(stream);
					beast::get_lowest_layer(s).expires_after(30s);
					co_await s.async_handshake(net::ssl::stream_base::client, asio_util::use_awaitable[ec]);
				}

				auto path = url.path_query();
				if (path.empty())
					path = "/";
				http::request<http::string_body> req{ option.verb, path, 11 };
				req.set(http::field::host, url.host());
				req.set(http::field::user_agent, "curl/7.82.0");
				#ifdef HAS_ZLIB
				req.set(http::field::accept_encoding, "gzip, deflate");
				#endif
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

				static_cast<http::response<http::string_body>&>(ret) = std::move(res);

				ret.code		 = static_cast<unsigned>(res.result());
				ret.content_type = ret[http::field::content_type];
#ifdef HAS_ZLIB
				if (ret[http::field::content_encoding] == "gzip")
				{
					beast::string_view compressd_body = static_cast<http::response<http::string_body>&>(ret).body();
					static_cast<http::response<http::string_body>&>(ret).body() = gzip::gunzip(compressd_body.data(), compressd_body.length());
				}
				else if (ret[http::field::content_encoding] == "deflate")
				{
					beast::string_view compressd_body = static_cast<http::response<http::string_body>&>(ret).body();

					std::string dest;
					dest.resize(2048);

					uLongf destLen = dest.size();
					uLong sourceLen = compressd_body.size();
					int zlib_ret = Z_OK;

					while(Z_MEM_ERROR == ( zlib_ret = uncompress2(reinterpret_cast<Bytef*>(dest.data()), &destLen, reinterpret_cast<const Bytef*>(compressd_body.data()), &sourceLen)))
					{
						dest.resize(dest.size()*2);
						destLen = dest.size();
					}
					if (zlib_ret == Z_DATA_ERROR)
					{
						ret.content_type = ret[http::field::content_type];
						ret.body		 = static_cast<http::response<http::string_body>&>(ret).body();
						break;
					}
					if (zlib_ret == Z_OK)
					{
						dest.resize(destLen);
						static_cast<http::response<http::string_body>&>(ret).body() = dest;
					}
				}
#endif // HAS_ZLIB
				ret.body		 = static_cast<http::response<http::string_body>&>(ret).body();

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

	inline awaitable<response_t> request(request_options_t options) { return detail::do_request(std::move(options)); }
}
