
#pragma once

#include <boost/asio/post.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/associated_executor.hpp>

namespace this_coro{

namespace detail {

	struct initiate_coro_yield_op
	{
		template<typename Handler>
		void operator()(Handler&& handler)
		{
			auto executor = boost::asio::get_associated_executor(handler);
			boost::asio::post(executor,
			[handler = std::move(handler)]() mutable
			{
				handler(boost::system::error_code());
			});
		}
	};

}

inline boost::asio::awaitable<void> coro_yield()
{
	return boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void(boost::system::error_code)>(detail::initiate_coro_yield_op{}, boost::asio::use_awaitable);
}

template<typename Handler>
auto coro_yield(Handler&& handler)
{
	return boost::asio::async_initiate<Handler, void(boost::system::error_code)>(detail::initiate_coro_yield_op{}, handler);
}

}