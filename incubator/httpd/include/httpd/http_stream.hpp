
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
    auto get_executor()
    {
        return std::visit([](auto && realtype){
            return realtype.get_executor();
        }, *this);
    }

    template<typename MutableBufferSequence, typename H>
    auto async_read_some(const MutableBufferSequence& b, H&& handler)
    {
        return std::visit([&b, handler = std::forward<H>(handler)](auto && realtype){
            return realtype.async_read_some(b, handler);
        }, *this);
    }

    template<typename ConstBufferSequence, typename H>
    auto async_write_some(const ConstBufferSequence& b, H&& handler)
    {
        return std::visit([&b, handler = std::forward<H>(handler)](auto && realtype){
            return realtype.async_write_some(b, handler);
        }, *this);
    }

};

}
