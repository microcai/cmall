
#include <iostream>
#include <list>
#include <filesystem>
#include <thread>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/cancellation_signal.hpp>

#include "utils/time_clock.hpp"
#include "utils/logging.hpp"

using steady_timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;

#include "persist_map.hpp"
#include "utils/coroyield.hpp"

#include "libmdbx/mdbx.h++"

#include "utils/logging.hpp"

using namespace mdbx;

static env::operate_parameters get_default_operate_parameters()
{
	env::operate_parameters db_param;

	db_param.max_maps = 4;
	db_param.reclaiming = env::reclaiming_options(MDBX_LIFORECLAIM);
	db_param.max_readers = std::thread::hardware_concurrency() + 1; // no more than std::thread::hardware_concurrency() plus one background_task

	return db_param;
}

struct persist_map_impl
{
	mutable boost::asio::thread_pool runners;
	steady_timer timer_;
	env_managed mdbx_env;
	map_handle mdbx_default_map;
	map_handle mdbx_lifetime_map;

	bool stop_flag = false;

	std::vector<std::thread> worker_threads;

	persist_map_impl(std::filesystem::path session_cache_file)
		: runners(std::thread::hardware_concurrency())
		, timer_(runners)
		, mdbx_env(session_cache_file, get_default_operate_parameters())
	{
		txn_managed t = mdbx_env.start_write();
		mdbx_default_map = t.create_map(NULL);
		t.put(mdbx_default_map, mdbx::slice("_persistmap"), mdbx::slice("persistmap"), upsert);
		mdbx_lifetime_map = t.create_map("lifetime");
		std::time_t _t;
		std::time(&_t);
		_t += 86400;
		t.put(mdbx_lifetime_map, mdbx::slice("_persistmap"), mdbx::slice(&_t, sizeof(_t)), upsert);
		t.commit();
		boost::asio::co_spawn(runners, background_task(), boost::asio::detached);
	}

	~persist_map_impl()
	{
		stop_flag = true;
		boost::system::error_code ignore_ec;
		timer_.cancel(ignore_ec);
		runners.join();
	}
	boost::asio::awaitable<void> background_task()
	{
		co_await this_coro::coro_yield();

		LOG_DBG << "background_task() running";
		while(!stop_flag)
		{
			try
			{
				std::time_t _now;
				std::time(&_now);

				std::list<mdbx::slice> outdated_keys;
				// TODO clean outdated key.
				txn_managed txn = mdbx_env.start_read();

				LOG_DBG << "background_task() open_cursor";

				cursor_managed c = txn.open_cursor(mdbx_lifetime_map);

				for (c.to_first(); !c.eof();  c.to_next(false))
				{
					cursor::move_result r = c.current();

					LOG_DBG << "iterator key:" << r.key.as_string();

					if (r)
					{
						std::time_t _expire_time;
						std::memcpy(&_expire_time, r.value.data(), sizeof(_expire_time));

						if (_expire_time < _now)
							outdated_keys.push_back(r.key);
					}
				}

				LOG_DBG << "background_task() close cursor";

				c.close();
				txn.commit();

				for (auto& k : outdated_keys)
				{
					txn = mdbx_env.start_write();
					txn.erase(mdbx_default_map, k);
					txn.erase(mdbx_lifetime_map, k);

					LOG_DBG << "drop key:" << k.as_string();

					txn.commit();
				}
			}
			catch(std::exception& e)
			{
				LOG_DBG << "background_task() exception: " << e.what();
			}

			if (stop_flag)
				co_return;

			timer_.expires_from_now(std::chrono::seconds(60));
			co_await timer_.async_wait(boost::asio::use_awaitable);
		}
	}

	boost::asio::awaitable<bool> has_key(std::string_view key) const
	{
		co_return co_await boost::asio::co_spawn(runners, [this, key]() mutable -> boost::asio::awaitable<bool>
		{
			try
			{
				txn_managed t = mdbx_env.start_read();
				t.get(mdbx_default_map, mdbx::slice(key));
				t.commit();
				co_return true;
			}
			catch(std::exception&)
			{
			}
			co_return false;
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<std::string> get(std::string_view key) const
	{
		co_return co_await boost::asio::co_spawn(runners, [this, key]() mutable -> boost::asio::awaitable<std::string>
		{
			txn_managed t = mdbx_env.start_read();
			mdbx::slice v = t.get(mdbx_default_map, mdbx::slice(key));
			std::string string_value = v.as_string();
			t.commit();
			co_return string_value;

		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<void> put(std::string_view key, std::string value, std::chrono::duration<int> lifetime)
	{
		std::time_t _expire_time;
		std::time(&_expire_time);
		_expire_time += lifetime.count();

		co_await boost::asio::co_spawn(runners, [this, key, _expire_time, value = std::move(value)]() mutable -> boost::asio::awaitable<void>
		{
			txn_managed t = mdbx_env.start_write();
			t.put(mdbx_default_map, mdbx::slice(key), mdbx::slice(value), upsert);
			t.put(mdbx_lifetime_map, mdbx::slice(key), mdbx::slice(&_expire_time, sizeof _expire_time), upsert);
			t.commit();

			co_return;
		}, boost::asio::use_awaitable);
	}
};

persist_map::~persist_map()
{
	impl().~persist_map_impl();
}

persist_map::persist_map(std::filesystem::path persist_file)
{
	static_assert(sizeof (obj_stor) >= sizeof (persist_map_impl));
	std::construct_at<persist_map_impl>(reinterpret_cast<persist_map_impl*>(obj_stor.data()), persist_file);
}

boost::asio::awaitable<bool> persist_map::has_key(std::string_view key) const
{
	co_return co_await impl().has_key(key);
}

boost::asio::awaitable<std::string> persist_map::get(std::string_view key) const
{
	co_return co_await impl().get(key);
}

boost::asio::awaitable<void> persist_map::put(std::string_view key, std::string value, std::chrono::duration<int> lifetime)
{
	co_return co_await impl().put(key, value, lifetime);
}

const persist_map_impl& persist_map::impl() const
{
	return *reinterpret_cast<const persist_map_impl*>(obj_stor.data());
}

persist_map_impl& persist_map::impl()
{
	return *reinterpret_cast<persist_map_impl*>(obj_stor.data());
}
