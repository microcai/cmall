
#include <chrono>
#include <boost/asio.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/composite_key.hpp>

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/tag.hpp>


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
	namespace tags{
		struct goods_ref{};
		struct merchant_id{};
		struct key_word{};
	}

	struct goods_ref
	{
		std::uint64_t merchant_id;
		std::string goods_id;
	};

	struct keywords_row
	{
		std::string key_word;
		double ranke;
		goods_ref target;

		std::uint64_t merchant_id() const { return target.merchant_id; }
		std::string_view goods_id() const { return target.goods_id; }

	};

	typedef boost::multi_index::multi_index_container<
		keywords_row,
		boost::multi_index::indexed_by<
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<tags::goods_ref>,
				boost::multi_index::composite_key<
					keywords_row,
					boost::multi_index::const_mem_fun<keywords_row, std::uint64_t, &keywords_row::merchant_id>,
					boost::multi_index::const_mem_fun<keywords_row, std::string_view, &keywords_row::goods_id>
				>
			>,
			boost::multi_index::hashed_non_unique<
				boost::multi_index::tag<tags::key_word>,
				boost::multi_index::member<
					keywords_row, std::string, &keywords_row::key_word
				>
			>,
			boost::multi_index::hashed_non_unique<
				boost::multi_index::tag<tags::merchant_id>,
				boost::multi_index::const_mem_fun<keywords_row, std::uint64_t, &keywords_row::merchant_id>
			>
		>
	> keywords_database;


	struct search_impl
	{
		search_impl(boost::asio::thread_pool& executor)
			: executor(executor)
			, indexer(boost::asio::co_spawn(executor, background_indexer(), use_promise))
		{
		}

		awaitable<void> background_indexer();

		awaitable<void> add_merchant(std::shared_ptr<repo_products>);
		awaitable<void> remove_merchant(std::uint64_t);
		awaitable<void> reindex_merchant(std::shared_ptr<repo_products>);

		boost::asio::thread_pool& executor;

		boost::asio::experimental::promise<void(std::exception_ptr)> indexer;

		keywords_database indexdb;
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
