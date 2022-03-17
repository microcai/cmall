
#pragma once

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>

#include "httpd/http_stream.hpp"

namespace httpd {
    boost::asio::awaitable<boost::system::error_code> send_string_response_body(http_any_stream& client,
        std::string& res_body, std::string expires, std::string mime_types, int http_version, bool keepalive);

}