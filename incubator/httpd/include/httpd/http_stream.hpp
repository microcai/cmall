
#pragma once

#include <variant>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

namespace httpd {

// 把 ssl 和 非 ssl 封成一个 variant.
template <typename... StreamTypes>
class http_stream : public std::variant<StreamTypes...>
{
public:
    using std::variant<StreamTypes...>::variant;
    typedef boost::asio::any_io_executor executor_type;

    auto get_executor()
    {
        return std::visit([](auto && realtype) mutable {
            return realtype.get_executor();
        }, *this);
    }

    template<typename MutableBufferSequence, typename H>
    auto async_read_some(const MutableBufferSequence& b, H&& handler)
    {
        return std::visit([&b, handler = std::forward<H>(handler)](auto && realtype) mutable {
            return realtype.async_read_some(b, std::forward<H>(handler));
        }, *this);
    }

    template<typename ConstBufferSequence, typename H>
    auto async_write_some(const ConstBufferSequence& b, H&& handler)
    {
        return std::visit([&b, handler = std::forward<H>(handler)](auto && realtype) mutable {
            return realtype.async_write_some(b, std::forward<H>(handler));
        }, *this);
    }

    auto close()
    {
        return std::visit([](auto && realtype) mutable {
            return boost::beast::get_lowest_layer(realtype).close();
        }, *this);
    }

    auto expires_after(auto expiry_time)
    {
        return std::visit([expiry_time](auto && realtype) mutable {
            return boost::beast::get_lowest_layer(realtype).expires_after(expiry_time);
        }, *this);
    }

    boost::asio::ip::tcp::socket& socket()
    {
        return std::visit([](auto && realtype) mutable -> boost::asio::ip::tcp::socket& {
            return boost::beast::get_lowest_layer(realtype).socket();
        }, *this);
    }

    template<class TeardownHandler>
    void async_teardown(boost::beast::role_type role, TeardownHandler&& handler)
    {
        std::visit([role, handler = std::forward<TeardownHandler>(handler)](auto&& realtype) mutable {
            boost::beast::async_teardown(role, realtype, std::forward<TeardownHandler>(handler));
        }, *this);
    }

};

typedef http_stream<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>> http_any_stream;


}

namespace boost::beast{

    template <typename... StreamTypes>
    void beast_close_socket(httpd::http_stream<StreamTypes...>& s)
    {
        s.close();
    }

    template<class TeardownHandler, typename... UnderlayingScokets>
    void async_teardown(
        role_type role,
        httpd::http_stream<UnderlayingScokets...>& socket,
        TeardownHandler&& handler)
    {
        socket.async_teardown(role, std::forward<TeardownHandler>(handler));
    }

}
