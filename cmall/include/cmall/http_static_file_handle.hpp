
#include <boost/asio/awaitable.hpp>
#include <boost/beast.hpp>

namespace cmall{
	boost::asio::awaitable<int> http_handle_static_file(size_t connection_id, boost::beast::http::request<boost::beast::http::string_body>&, boost::beast::tcp_stream& client);
}
