﻿
#include <boost/asio/awaitable.hpp>
#include <boost/beast.hpp>

#include "httpd/http_stream.hpp"

using boost::asio::awaitable;

namespace cmall{
	awaitable<int> http_handle_static_file(size_t connection_id, boost::beast::http::request<boost::beast::http::string_body>&, httpd::http_any_stream& client);
}
