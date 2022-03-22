
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

	bool operator < (const goods_ref & a , const goods_ref & b)
	{
		return a.merchant_id < b.merchant_id && a.goods_id < b.goods_id;
	}

	namespace tags{
		struct goods_ref{};
		struct merchant_id{};
		struct key_word{};
	}

	struct keyword_index_item
	{
		std::string key_word;
		double ranke;
		goods_ref target;

		std::uint64_t merchant_id() const { return target.merchant_id; }
		std::string_view goods_id() const { return target.goods_id; }
	};

	typedef boost::multi_index::multi_index_container<
		keyword_index_item,
		boost::multi_index::indexed_by<
			boost::multi_index::hashed_non_unique<
				boost::multi_index::tag<tags::goods_ref>,
				boost::multi_index::composite_key<
					keyword_index_item,
					boost::multi_index::const_mem_fun<keyword_index_item, std::uint64_t, &keyword_index_item::merchant_id>,
					boost::multi_index::const_mem_fun<keyword_index_item, std::string_view, &keyword_index_item::goods_id>
				>
			>,
			boost::multi_index::hashed_non_unique<
				boost::multi_index::tag<tags::key_word>,
				boost::multi_index::member<
					keyword_index_item, std::string, &keyword_index_item::key_word
				>
			>,
			boost::multi_index::hashed_non_unique<
				boost::multi_index::tag<tags::merchant_id>,
				boost::multi_index::const_mem_fun<keyword_index_item, std::uint64_t, &keyword_index_item::merchant_id>
			>
		>
	> keywords_database;


	struct search_impl
	{
		void add_merchant_unlocked(std::vector<product> products)
		{
			auto &goods_ref_containr = indexdb.get<tags::goods_ref>();

			for (auto& p : products)
			{
				keyword_index_item item;

				item.target = { .merchant_id = p.merchant_id, .goods_id = p.product_id };

				for (auto& keyword : p.keywords)
				{
					item.key_word = keyword.keyword;
					item.ranke = keyword.rank;
					goods_ref_containr.insert(item);
				}
			}
		}

		awaitable<void> add_merchant(std::shared_ptr<repo_products> repo)
		{
			auto products = co_await repo->get_products();

			std::unique_lock<std::shared_mutex> l(dbmtx);

			add_merchant_unlocked(products);
			co_return;
		}

		awaitable<std::vector<goods_ref>> search_goods(std::string search_string)
		{
			std::vector<goods_ref> result;

			std::map<goods_ref, double> result_ranking;

			std::shared_lock<std::shared_mutex> l(dbmtx);

			auto &keyword_ref_containr = indexdb.get<tags::key_word>();

			auto keyword_result = keyword_ref_containr.equal_range(search_string);

			for (const auto& item : boost::make_iterator_range(keyword_result.first, keyword_result.second) )
			{
				result_ranking[item.target] += item.ranke;
			}

			l.unlock();

			std::vector<std::pair<goods_ref, double>> vectored_result;
			for (auto & i : result_ranking)
				vectored_result.push_back(i);

			sort(vectored_result.begin(), vectored_result.end(), [](auto & a, auto& b){ return b.second < a.second; });

			for (int i=0; i < 10; i++)
			{
				if (i < vectored_result.size())
					result.push_back(vectored_result[i].first);
				else break;
			}

			co_return result;
		}

		awaitable<void> remove_merchant(std::uint64_t);

		awaitable<void> reload_merchant(std::shared_ptr<repo_products> repo)
		{
			auto products = co_await repo->get_products();

			std::unique_lock<std::shared_mutex> l(dbmtx);

			indexdb.get<tags::merchant_id>().erase(repo->get_merchant_uid());

			add_merchant_unlocked(products);
			co_return;
		}

		mutable std::shared_mutex dbmtx;
		keywords_database indexdb;
	};

	search::search()
	{
		static_assert(sizeof(obj_stor) >= sizeof(search_impl));
		std::construct_at(reinterpret_cast<search_impl*>(obj_stor.data()));
	}

	search::~search()
	{
		std::destroy_at(reinterpret_cast<search_impl*>(obj_stor.data()));
	}

	awaitable<void> search::add_merchant(std::shared_ptr<repo_products> merchant_repos)
	{
		return impl().add_merchant(merchant_repos);
	}

	awaitable<void> search::reload_merchant(std::shared_ptr<repo_products> repo)
	{
		return impl().reload_merchant(repo);
	}

	awaitable<std::vector<goods_ref>> search::search_goods(std::string q)
	{
		return impl().search_goods(q);
	}

	const search_impl& search::impl() const
	{
		return *reinterpret_cast<const search_impl*>(obj_stor.data());
	}

	search_impl& search::impl()
	{
		return *reinterpret_cast<search_impl*>(obj_stor.data());
	}
}
