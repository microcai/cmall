
#include "boost/asio/buffer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/read_until.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <boost/process/extend.hpp>
#include <boost/process/handles.hpp>
#include <chrono>
#include <cstdlib>

#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost::asio::experimental::awaitable_operators;

#include "services/search_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"
#include "cmall/error_code.hpp"
#include "utils/uawaitable.hpp"

#include "services/repo_products.hpp"

using boost::asio::use_awaitable;
using boost::asio::experimental::use_promise;
using boost::asio::awaitable;

namespace services
{
	struct search_impl
	{
		search_impl(boost::asio::thread_pool& executor)
			: executor(executor)
			, indexer(boost::asio::co_spawn(executor, background_indexer(), use_promise))
		{
		}

		awaitable<void> background_indexer();

		boost::asio::thread_pool& executor;

		boost::asio::experimental::promise<void(std::exception_ptr)> indexer;

		std::map<std::uint64_t, std::shared_ptr<repo_products>> loaded_repos;
	};

	search::search(boost::asio::thread_pool& executor)
	{
		static_assert(sizeof(obj_stor) >= sizeof(search_impl));
		std::construct_at(reinterpret_cast<search_impl*>(obj_stor.data()), executor);
	}

	search::~search()
	{
		std::destroy_at(reinterpret_cast<search_impl*>(obj_stor.data()));
	}

	const search_impl& search::impl() const
	{
		return *reinterpret_cast<const search_impl*>(obj_stor.data());
	}

	search_impl& search::impl()
	{
		return *reinterpret_cast<search_impl*>(obj_stor.data());
	}

	awaitable<void> search_impl::background_indexer()
	{
		co_return;
	}

}
