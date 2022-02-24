

#include <iostream>
#include <list>
#include <filesystem>
#include <thread>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/json.hpp>

#include "utils/time_clock.hpp"
#include "utils/logging.hpp"

using steady_timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;

#include "services/persist_session.hpp"
#include "persist_map.hpp"

namespace services {
struct persist_session_impl
{
	persist_map mdbx_db;

	persist_session_impl(std::filesystem::path session_cache_file)
		: mdbx_db(session_cache_file)
	{
	}

	~persist_session_impl()
	{
	}
		boost::asio::awaitable<bool> exist(std::string_view session_id) const
		{
			return mdbx_db.has_key(session_id);
		}

		boost::asio::awaitable<client_session> load(std::string_view session_id) const
		{
			client_session cs;
			auto jv = boost::json::parse(co_await mdbx_db.get(session_id), {}, { 64, false, false, true });

			// TODO reload saved info from jv
			cs.session_id = session_id;
			//ci.cap =;
			// TODO
// 				ci.db_user_entry = m_database.async_load_user_entry();

			co_return cs;
		}

		boost::asio::awaitable<void> save(std::string_view session_id, const client_session& session, std::chrono::duration<int> lifetime)
		{
			//TODO, 用　mdbx 保存 session
			co_return;
		}

		boost::asio::awaitable<void> update_lifetime(std::string_view session_id, std::chrono::duration<int> lifetime)
		{
			//TODO, 更新　mdbx 里保存的 session
			co_return;
		}
};

persist_session::~persist_session()
{
	impl().~persist_session_impl();
}

persist_session::persist_session(std::filesystem::path persist_file)
{
	static_assert(sizeof (obj_stor) >= sizeof (persist_session_impl));
	std::construct_at<persist_session_impl>(reinterpret_cast<persist_session_impl*>(obj_stor.data()), persist_file);
}

boost::asio::awaitable<bool> persist_session::exist(std::string_view key) const
{
	return impl().exist(key);
}

boost::asio::awaitable<client_session> persist_session::load(std::string_view session_id) const
{
	return impl().load(session_id);
}

boost::asio::awaitable<void> persist_session::save(std::string_view session_id, const client_session& session, std::chrono::duration<int> lifetime)
{
	return impl().save(session_id, session, lifetime);
}

boost::asio::awaitable<void> persist_session::update_lifetime(std::string_view session_id, std::chrono::duration<int> lifetime)
{
	return impl().update_lifetime(session_id, lifetime);
}

const persist_session_impl& persist_session::impl() const
{
	return *reinterpret_cast<const persist_session_impl*>(obj_stor.data());
}

persist_session_impl& persist_session::impl()
{
	return *reinterpret_cast<persist_session_impl*>(obj_stor.data());
}

}