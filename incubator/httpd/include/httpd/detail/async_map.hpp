
#pragma once

#include <concepts>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/promise.hpp>

// async.js like map()

#include "./concepts.hpp"

namespace httpd::detail {

    template<typename Range, typename Operand> requires  (is_coroutine_function_with_args<Operand, typename std::decay_t<Range>::value_type> && is_range<Range> )
    boost::asio::awaitable<void> map(Range&& range, Operand func)
    {
		std::vector<boost::asio::experimental::promise<void(std::exception_ptr)>> co_threads;
		for (auto&& a: range)
        {
            // 如果对象有 executor 则放到对象自己的 executor 里执行 func
            // 如果对象没 executor 则放到当前协程自己的 executor 里执行 func
            if constexpr ( is_asio_io_object<decltype(a)> )
            {
			    co_threads.push_back(boost::asio::co_spawn(a.get_executor(), func(a), boost::asio::experimental::use_promise));
            }
            else
            {
			    co_threads.push_back(boost::asio::co_spawn(co_await boost::asio::this_coro::executor, func, boost::asio::experimental::use_promise));
            }
        }

        // 然后等待所有的协程工作完毕.
        for (auto&& co : co_threads)
            co_await co.async_wait(boost::asio::use_awaitable);
    }
}