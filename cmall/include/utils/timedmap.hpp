
#pragma once

#include <thread>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <chrono>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/timer/timer.hpp>

#include "time_clock.hpp"
#include "coroyield.hpp"

namespace utility
{

	template<typename value_type, typename key_type>
	requires requires {
		std::is_trivially_constructible<key_type>::value;
	}
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

		struct some_shared_data{
			boost::asio::cancellation_signal cancell_signal;
			bool stop_flag = false;
		};

		std::shared_ptr<some_shared_data> shared_data_;
		std::shared_mutex mtx;

	public:
		template<typename Executor>
		timedmap(Executor&& io, std::chrono::milliseconds lifetime)
			: shared_data_(std::make_shared<some_shared_data>())
		{
			boost::asio::co_spawn(io, pruge_thread(lifetime, shared_data_), boost::asio::detached);
		}

		~timedmap()
		{
			shared_data_->stop_flag = true;
			shared_data_->cancell_signal.emit(boost::asio::cancellation_type::terminal);
		}

		std::optional<value_type> get(key_type key)
		{
			std::shared_lock<std::shared_mutex> readlock(mtx);

			auto it = this->stor.template get<tag_key>().find(key);

			if (it!= this->stor.template get<tag_key>().end())
				return it->value;
			return {};
		}

		void put(key_type k, value_type v)
		{
			std::unique_lock<std::shared_mutex> writelock(mtx);

			auto& ordered_by_key = this->stor.template get<tag_key>();

			container_item_type cv;
			cv.key = k;
			cv.value = v;
			cv.insert_time = time_clock::steady_clock::now();

			ordered_by_key.insert(cv);
		}

	private:
		boost::asio::awaitable<void> pruge_thread(std::chrono::milliseconds lifetime, std::shared_ptr<some_shared_data> shared_data)
		{
			co_await this_coro::coro_yield();
			while (! shared_data->stop_flag)
			{
				boost::timer::cpu_timer avoid_stucker;

				auto now		= time_clock::steady_clock::now();

				auto pruge_time = now - lifetime;
				std::chrono::milliseconds diff = std::chrono::milliseconds(30000);
				{
					std::unique_lock<std::shared_mutex> writelock(mtx);

					auto & ordered_by_insert_time = stor.template get<tag_insert_time>();

					auto last_to_erase = ordered_by_insert_time.upper_bound(pruge_time);

					for (auto it = ordered_by_insert_time.begin(); it != last_to_erase; )
					{
						diff = it->insert_time - pruge_time;
						ordered_by_insert_time.erase(it++);
					}
				}

				if (shared_data->stop_flag)
					co_return;

				boost::asio::basic_waitable_timer<time_clock::steady_clock> timer(co_await boost::asio::this_coro::executor);

				timer.expires_from_now(diff);
				co_await timer.async_wait(boost::asio::bind_cancellation_slot(shared_data->cancell_signal.slot(),  boost::asio::use_awaitable));

			}
		}

	};

}