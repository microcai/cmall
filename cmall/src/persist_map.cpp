
#include <filesystem>
#include "persist_map.hpp"

#include "libmdbx/mdbx.h++"

using namespace mdbx;

struct persist_map_impl
{
	env_managed mdbx_env;
	map_handle mdbx_map;

	persist_map_impl(std::filesystem::path session_cache_file)
	  :  mdbx_env(session_cache_file, env::operate_parameters{})
	{
		txn_managed t = mdbx_env.start_write();
		mdbx_map = t.create_map(NULL);
		t.put(mdbx_map, mdbx::slice("_persistmap"), mdbx::slice("persistmap"), insert_unique);
		t.commit();
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
{}

persist_map::persist_map(std::filesystem::path persist_file)
{
	impl.reset(new persist_map_impl(persist_file));
}

boost::asio::awaitable<std::string> persist_map::get(std::string_view key) const
{
	return impl->get(key);
}

boost::asio::awaitable<void> persist_map::put(std::string_view key, std::string value, std::chrono::duration<int> lifetime)
{
	return impl->put(key, value, lifetime);
}
