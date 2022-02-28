
#pragma once

#include <iostream>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/timer/timer.hpp>

#include "time_clock.hpp"
#include "coroyield.hpp"

namespace utility
{

	template<typename value_type, typename key_type>
	class timedmap
	{
		struct container_item_type
		{
			key_type key;
			value_type value;
			time_clock::steady_clock::time_point insert_time;
		};

		struct tag_key{};
		struct tag_insert_time{};

		typedef boost::multi_index::multi_index_container<
			container_item_type,
			boost::multi_index::indexed_by<
				boost::multi_index::sequenced<>,
				boost::multi_index::ordered_non_unique<
					boost::multi_index::tag<tag_insert_time>,
					boost::multi_index::member<container_item_type, time_clock::steady_clock::time_point, &container_item_type::insert_time>
				>,
				boost::multi_index::ordered_unique<
					boost::multi_index::tag<tag_key>,
					boost::multi_index::member<container_item_type, key_type, &container_item_type::key>
				>
			>
		> map_t;

		map_t stor;

		boost::asio::cancellation_signal cancell_signal;

	public:
		template<typename Executor>
		timedmap(Executor&& io, std::chrono::milliseconds lifetime)
		{
			boost::asio::co_spawn(io, pruge_thread(lifetime), boost::asio::bind_cancellation_slot(cancell_signal.slot(), boost::asio::detached));
		}

		~timedmap()
		{
			cancell_signal.emit(boost::asio::cancellation_type::terminal);
		}

		std::optional<value_type> get(key_type key)
		{
			auto it = this->stor.template get<tag_key>().find(key);

			if (it!= this->stor.template get<2>().end())
				return it->value;
			return {};
		}

		void put(key_type k, value_type v);

	private:

		static boost::asio::awaitable<bool> check_canceled()
		{
			boost::asio::cancellation_state cs = co_await boost::asio::this_coro::cancellation_state;
			if (cs.cancelled() != boost::asio::cancellation_type::none)
				throw boost::system::system_error(boost::asio::error::operation_aborted);
			co_return false;
		}

		boost::asio::awaitable<void> pruge_thread(std::chrono::milliseconds lifetime)
		{
			co_await this_coro::coro_yield();
			while (!(co_await check_canceled()))
			{
				boost::timer::cpu_timer avoid_stucker;
				auto pruge_time = time_clock::steady_clock::now() - lifetime;

				auto & ordered_by_insert_time = stor.template get<tag_insert_time>();

				auto last_to_erase = ordered_by_insert_time.lower_bound(pruge_time);

				for (auto it = ordered_by_insert_time.begin(); it != last_to_erase; co_await check_canceled())
				{
					ordered_by_insert_time.erase(it++);
				}

				boost::asio::basic_waitable_timer<time_clock::steady_clock> timer(co_await boost::asio::this_coro::executor);
				co_await check_canceled();
				timer.expires_from_now(lifetime/2);
				co_await timer.async_wait(boost::asio::bind_cancellation_slot(cancell_signal.slot(), boost::asio::use_awaitable));

			}
			co_return;
		}


	};

}