
#include "stdafx.hpp"

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

#include "services/merchant_git_repo.hpp"

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

		std::string_view keyword() const { return key_word; }
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
				boost::multi_index::const_mem_fun<
					keyword_index_item, std::string_view, &keyword_index_item::keyword
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

		awaitable<void> add_merchant(std::shared_ptr<merchant_git_repo> repo)
		{
			// 这里只是做索引, 不用在意店铺名字.
			auto head = co_await repo->git_head();
			auto products = co_await repo->get_products("any_merchant", "");

			std::unique_lock<std::shared_mutex> l(dbmtx);

			merchant_git_snap[repo->get_merchant_uid()] = head;
			add_merchant_unlocked(products);
			co_return;
		}

		awaitable<std::vector<goods_ref>> search_goods(std::vector<std::string> search_strings)
		{
			typedef boost::multi_index::multi_index_container<
				goods_ref_with_ranking,
				boost::multi_index::indexed_by<
					boost::multi_index::sequenced<>,
					boost::multi_index::ranked_unique<
						boost::multi_index::composite_key<
							goods_ref_with_ranking,
							boost::multi_index::member<goods_ref_with_ranking, std::uint64_t, &goods_ref_with_ranking::merchant_id>,
							boost::multi_index::member<goods_ref_with_ranking, std::string, &goods_ref_with_ranking::goods_id>
						>
					>,
					boost::multi_index::ranked_non_unique<
						boost::multi_index::member<
							goods_ref_with_ranking, double, &goods_ref_with_ranking::ranking
						>
					>
				>
			> goods_ref_with_ranking_container;

			std::vector<goods_ref> result;

			goods_ref_with_ranking_container result_tmp;

			std::shared_lock<std::shared_mutex> l(dbmtx);

			auto &keyword_ref_containr = indexdb.get<tags::key_word>();

			for (std::string search_string: search_strings)
			{
				auto keyword_result = keyword_ref_containr.equal_range(search_string);

				LOG_DBG << "search {" << search_string << "}, has " << std::distance(keyword_result.first, keyword_result.second) << " results.";

				for (const auto& item : boost::make_iterator_range(keyword_result.first, keyword_result.second) )
				{
					goods_ref_with_ranking new_item = { .merchant_id = item.target.merchant_id, .goods_id = item.target.goods_id, .ranking = 0-item.ranke };
					auto insertion_result = result_tmp.push_back(new_item);
					if (insertion_result.second == false)
					{
						result_tmp.modify(insertion_result.first, [&](goods_ref_with_ranking& old){ old.ranking -= item.ranke, old.ranking -= search_strings.size();});
					}
				}
			}

			l.unlock();

			LOG_DBG << "search {" << search_strings[0] << ",...}, has " << result_tmp.size() << " results sorted.";

			for (auto item: result_tmp.get<2>())
			{
				if (result.size() < 80)
					result.push_back(goods_ref{.merchant_id = item.merchant_id, .goods_id = item.goods_id });
				else break;
			}

			LOG_DBG << "search {" << search_strings[0] << ",...}, has " << result.size() << " results returned.";
			co_return result;
		}

		awaitable<std::vector<std::string>> keywords_list(std::uint64_t merchant_id)
		{
			std::set<std::string> keyword_set;

			std::shared_lock<std::shared_mutex> l(dbmtx);

			auto merchant_it = indexdb.get<tags::merchant_id>().equal_range(merchant_id);

			for (auto items : boost::make_iterator_range(merchant_it.first, merchant_it.second))
			{
				keyword_set.insert(items.key_word);
			}
			l.unlock();

			std::vector<std::string> result;

			for (auto k : keyword_set)
				result.push_back(k);

			co_return result;
		}

		awaitable<void> remove_merchant(std::uint64_t);

		awaitable<void> reload_merchant(std::shared_ptr<merchant_git_repo> repo)
		{
			auto head = co_await repo->git_head();
			auto products = co_await repo->get_products("any_name_be_ok", "");

			std::unique_lock<std::shared_mutex> l(dbmtx);

			indexdb.get<tags::merchant_id>().erase(repo->get_merchant_uid());
			add_merchant_unlocked(products);
			merchant_git_snap[repo->get_merchant_uid()] = head;
			co_return;
		}

		awaitable<std::string> cached_head(std::shared_ptr<merchant_git_repo> repo)
		{
			std::unique_lock<std::shared_mutex> l(dbmtx);
			co_return merchant_git_snap[repo->get_merchant_uid()];
		}

		mutable std::shared_mutex dbmtx;
		keywords_database indexdb;
		std::map<std::uint64_t, std::string> merchant_git_snap;
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

	awaitable<void> search::add_merchant(std::shared_ptr<merchant_git_repo> merchant_repos)
	{
		return impl().add_merchant(merchant_repos);
	}

	awaitable<std::string> search::cached_head(std::shared_ptr<merchant_git_repo> repo)
	{
		return impl().cached_head(repo);
	}

	awaitable<void> search::reload_merchant(std::shared_ptr<merchant_git_repo> repo)
	{
		return impl().reload_merchant(repo);
	}

	awaitable<std::vector<goods_ref>> search::search_goods(std::vector<std::string> q)
	{
		return impl().search_goods(q);
	}

	awaitable<std::vector<std::string>> search::keywords_list(std::uint64_t merchant_id)
	{
		return impl().keywords_list(merchant_id);
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
