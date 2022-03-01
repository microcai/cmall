
#include <iostream>
#include <list>
#include <filesystem>
#include <thread>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/cancellation_signal.hpp>

#include "utils/time_clock.hpp"
#include "utils/logging.hpp"

using steady_timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;

#include "persist_map.hpp"
#include "utils/coroyield.hpp"

#include "libmdbx/mdbx.h++"

using namespace mdbx;

static env::operate_parameters get_default_operate_parameters()
{
	env::operate_parameters db_param;

	db_param.max_maps = 4;
	db_param.reclaiming = env::reclaiming_options(MDBX_LIFORECLAIM);

	return db_param;
}

struct persist_map_impl
{
	boost::asio::io_context ioc;
	std::shared_ptr<boost::asio::io_context::work> work;
	std::vector<std::thread> runners;

	env_managed mdbx_env;
	map_handle mdbx_default_map;
	map_handle mdbx_lifetime_map;

	boost::asio::cancellation_signal m_cancell_signal;

	persist_map_impl(std::filesystem::path session_cache_file)
		: work(std::make_shared<boost::asio::io_context::work>(ioc))
		, mdbx_env(session_cache_file, get_default_operate_parameters())
	{
		boost::asio::co_spawn(ioc, background_task(), boost::asio::bind_cancellation_slot(m_cancell_signal.slot(), boost::asio::detached));
		txn_managed t = mdbx_env.start_write();
		mdbx_default_map = t.create_map(NULL);
		t.put(mdbx_default_map, mdbx::slice("_persistmap"), mdbx::slice("persistmap"), upsert);
		mdbx_lifetime_map = t.create_map("lifetime");
		std::time_t _t;
		std::time(&_t);
		_t += 86400;
		t.put(mdbx_lifetime_map, mdbx::slice("_persistmap"), mdbx::slice(&_t, sizeof(_t)), upsert);
		t.commit();

		for (unsigned i = 0;  i < std::thread::hardware_concurrency(); i++)
			runners.push_back(std::thread(boost::bind(&boost::asio::io_context::run, &ioc)));
	}

	~persist_map_impl()
	{
		work.reset();

		m_cancell_signal.emit(boost::asio::cancellation_type::terminal);

		for (auto & runner : runners)
			runner.join();
	}

	static boost::asio::awaitable<bool> check_canceled()
	{
		boost::asio::cancellation_state cs = co_await boost::asio::this_coro::cancellation_state;
		if (cs.cancelled() != boost::asio::cancellation_type::none)
			throw boost::system::system_error(boost::asio::error::operation_aborted);
		co_return false;
	}

	boost::asio::awaitable<void> background_task()
	{
		co_await this_coro::coro_yield();

		std::cerr << "background_task() running\n";
		for (co_await check_canceled(); !co_await check_canceled(); co_await check_canceled())
		{
			try
			{
				std::time_t _now;
				std::time(&_now);

				std::list<mdbx::slice> outdated_keys;
				// TODO clean outdated key.
				txn_managed txn = mdbx_env.start_read();

				std::cerr << "background_task() open_cursor\n";

				cursor_managed c = txn.open_cursor(mdbx_lifetime_map);

				for (c.to_first(); !c.eof();  c.to_next(false))
				{
					cursor::move_result r = c.current();

					std::cerr << "iterator key:" << r.key.as_string() << "\n";

					if (r)
					{
						std::time_t _expire_time;
						std::memcpy(&_expire_time, r.value.data(), sizeof(_expire_time));

						if (_expire_time < _now)
							outdated_keys.push_back(r.key);
					}
				}

				std::cerr << "background_task() close cursor\n";

				c.close();
				txn.commit();

				for (auto& k : outdated_keys)
				{
					txn = mdbx_env.start_write();
					txn.erase(mdbx_default_map, k);
					txn.erase(mdbx_lifetime_map, k);

					std::cerr << "drop key:" << k.as_string() << "\n";

					txn.commit();
				}
			}
			catch(std::exception& e)
			{
				std::cerr << "background_task() exception: " << e.what()  << "\n";
			}

			co_await check_canceled();

			steady_timer timer(co_await boost::asio::this_coro::executor);
			timer.expires_from_now(std::chrono::seconds(60));
			co_await timer.async_wait(boost::asio::bind_cancellation_slot(m_cancell_signal.slot(), boost::asio::use_awaitable));
		}
	}

	boost::asio::awaitable<bool> has_key(std::string_view key) const
	{
		try
		{
			txn_managed t = mdbx_env.start_read();
			t.get(mdbx_default_map, mdbx::slice(key));
			t.commit();
			co_return true;
		}catch(std::exception&)
		{
			co_return false;
		}
	}

	boost::asio::awaitable<std::string> get(std::string_view key) const
	{
		txn_managed t = mdbx_env.start_read();
		mdbx::slice v = t.get(mdbx_default_map, mdbx::slice(key));
		auto string_value = v.as_string();
		t.commit();
		co_return string_value;
	}

	boost::asio::awaitable<void> put(std::string_view key, std::string value, std::chrono::duration<int> lifetime)
	{
		std::time_t _expire_time;
		std::time(&_expire_time);
		_expire_time += lifetime.count();

		co_await boost::asio::co_spawn(ioc, [this, key, _expire_time, value = std::move(value)]() mutable -> boost::asio::awaitable<void>
		{
			txn_managed t = mdbx_env.start_write();
			t.put(mdbx_default_map, mdbx::slice(key), mdbx::slice(value), upsert);
			t.put(mdbx_lifetime_map, mdbx::slice(key), mdbx::slice(&_expire_time, sizeof _expire_time), upsert);
			t.commit();

			std::cerr << "commit ok?\n";

			co_return;
		}, boost::asio::use_awaitable);

		co_return;
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
	return impl().has_key(key);
}

boost::asio::awaitable<std::string> persist_map::get(std::string_view key) const
{
	return impl().get(key);
}

boost::asio::awaitable<void> persist_map::put(std::string_view key, std::string value, std::chrono::duration<int> lifetime)
{
	return impl().put(key, value, lifetime);
}

const persist_map_impl& persist_map::impl() const
{
	return *reinterpret_cast<const persist_map_impl*>(obj_stor.data());
}

persist_map_impl& persist_map::impl()
{
	return *reinterpret_cast<persist_map_impl*>(obj_stor.data());
}
