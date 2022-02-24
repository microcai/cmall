
#include <iostream>
#include <list>
#include <filesystem>
#include <thread>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/promise.hpp>
#include "utils/time_clock.hpp"
#include "utils/logging.hpp"

using steady_timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;

#include "persist_map.hpp"

#include "libmdbx/mdbx.h++"

using namespace mdbx;

static env::operate_parameters get_default_operate_parameters()
{
	env::operate_parameters db_param;

	db_param.max_maps = 0;
	db_param.reclaiming = env::reclaiming_options(MDBX_LIFORECLAIM);

	return db_param;
}

struct persist_map_impl
{
	boost::asio::io_context ioc;
	std::shared_ptr<boost::asio::io_context::work> work;
	std::vector<std::thread> runners;

	env_managed mdbx_env;
	map_handle mdbx_map;

	steady_timer m_task_timer;
	boost::asio::experimental::promise<void(std::exception_ptr)> background_task_coro;

	persist_map_impl(std::filesystem::path session_cache_file)
		: work(std::make_shared<boost::asio::io_context::work>(ioc))
		, mdbx_env(session_cache_file, get_default_operate_parameters())
		, m_task_timer(ioc)
		, background_task_coro(boost::asio::co_spawn(ioc, background_task(), boost::asio::experimental::use_promise))
	{
		txn_managed t = mdbx_env.start_write();
		mdbx_map = t.create_map(NULL);
		t.put(mdbx_map, mdbx::slice("_persistmap"), mdbx::slice("persistmap"), upsert);
		t.commit();

		for (int i = 0;  i < std::thread::hardware_concurrency(); i++)
			runners.push_back(std::thread(boost::bind(&boost::asio::io_context::run, &ioc)));
	}

	~persist_map_impl()
	{
		work.reset();

		boost::system::error_code ec;
		m_task_timer.cancel(ec);

		background_task_coro.cancel();

		for (auto & runner : runners)
			runner.join();
	}

	boost::asio::awaitable<void> background_task()
	{
		std::cerr << "background_task() running\n";
		for (;;)
		{
			try
			{
				std::list<mdbx::slice> outdated_keys;
				// TODO clean outdated key.
				txn_managed txn = mdbx_env.start_read();

				std::cerr << "background_task() open_cursor\n";

				cursor_managed c = txn.open_cursor(mdbx_map);

				for (c.to_first(); !c.eof();  c.to_next(false))
				{
					cursor::move_result r = c.current();

					std::cerr << "iterator key:" << r.key.as_string() << "\n";

					if (r)
					{
						// TODO 从 value 里提取生成日期.
						// TODO 然后把过期的时间

						outdated_keys.push_back(r.key);
					}
				}

				std::cerr << "background_task() close cursor\n";

				c.close();
				txn.commit();

				for (auto& k : outdated_keys)
				{
					txn = mdbx_env.start_write();
					txn.erase(mdbx_map, k);

					std::cerr << "drop key:" << k.as_string() << "\n";

					txn.commit();
				}
			}
			catch(std::exception& e)
			{
				std::cerr << "background_task() exception: " << e.what()  << "\n";
			}
			co_await boost::asio::this_coro::throw_if_cancelled();

			m_task_timer.expires_from_now(std::chrono::seconds(60));
			co_await m_task_timer.async_wait(boost::asio::use_awaitable);
		}
	}

	boost::asio::awaitable<std::string> get(std::string_view key) const
	{
		txn_managed t = mdbx_env.start_read();
		mdbx::slice v = t.get(mdbx_map, mdbx::slice(key));
		auto string_value = v.as_string();
		t.commit();
		co_return string_value;
	}

	boost::asio::awaitable<void> put(std::string_view key, std::string value, std::chrono::duration<int> lifetime)
	{
		txn_managed t = mdbx_env.start_write();
		t.put(mdbx_map, mdbx::slice(key), mdbx::slice(value), upsert);
		t.commit();
		co_return;
	}
};

persist_map::~persist_map()
{
	impl().~persist_map_impl();
}

persist_map::persist_map(std::filesystem::path persist_file)
{
	std::construct_at<persist_map_impl>(reinterpret_cast<persist_map_impl*>(obj_stor.data()), persist_file);
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