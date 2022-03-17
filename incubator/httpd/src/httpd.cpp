
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/asio/awaitable.hpp>

#include "httpd/httpd.hpp"

boost::asio::awaitable<boost::system::error_code> httpd::send_string_response_body(http_any_stream& client,
        std::string& res_body, std::string expires, std::string mime_types, int http_version, bool keepalive)
{
    boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::ok, http_version };

    res.set(boost::beast::http::field::server, "cmall1.0");
    res.set(boost::beast::http::field::expires, expires);
    res.set(boost::beast::http::field::content_type, mime_types);
    res.keep_alive(keepalive);

    boost::system::error_code ec;
    res.body() = res_body;

    res.prepare_payload();

    boost::beast::http::response_serializer<boost::beast::http::string_body, boost::beast::http::fields> sr{ res };

    co_await boost::beast::http::async_write(client, sr, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return ec;
}