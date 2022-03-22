
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

#include "services/repo_products.hpp"

using boost::asio::awaitable;

namespace services
{
	struct search_impl;
	class search
	{
	public:
		search(boost::asio::thread_pool& executor);
		~search();

		awaitable<void> add_merchant(std::shared_ptr<repo_products>);

	private:
		const search_impl& impl() const;
		search_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
