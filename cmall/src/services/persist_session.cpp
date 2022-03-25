
#include "stdafx.hpp"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/bind.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <iostream>
#include <list>
#include <thread>

#include "utils/logging.hpp"
#include "utils/time_clock.hpp"

using steady_timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;

#include "persist_map.hpp"
#include "services/persist_session.hpp"

#include "cmall/js_util.hpp"

static inline std::string json_to_string(const boost::json::value& jv, bool lf = true)
{
	if (lf) return boost::json::serialize(jv) + "\n";
	return boost::json::serialize(jv);
}

namespace services
{
	struct persist_session_impl
	{
		persist_map mdbx_db;

		persist_session_impl(std::filesystem::path session_cache_file)
			: mdbx_db(session_cache_file)
		{
		}

		~persist_session_impl() { }
		awaitable<bool> exist(std::string_view session_id) const
		{
			co_return co_await mdbx_db.has_key(session_id);
		}

		awaitable<client_session> load(std::string_view session_id) const
		{
			client_session cs;
			boost::json::object jv;

			jv = boost::json::parse(co_await mdbx_db.get(session_id), {}, { 64, false, false, true }).as_object();

			// TODO reload saved info from jv
			cs.session_id = session_id;

			cs.verify_telephone = jsutil::json_as_string(jv["verifyinfo"].as_object()["tel"]);
			auto verify_cookie = jsutil::json_as_string(jv["verifyinfo"].as_object()["verify_cookie"]);
			if (!verify_cookie.empty())
				cs.verify_session_cookie = verify_session_access::from_string(verify_cookie);

			if (jv.contains("uid"))
			{
				cs.user_info = cmall_user{};
				cs.user_info->uid_ = jv.at("uid").as_int64();
			}

			co_return cs;
		}

		awaitable<void> save(
			std::string_view session_id, const client_session& session, std::chrono::duration<int> lifetime)
		{
			// TODO, 用　mdbx 保存 session

			boost::json::object ser;
			if (session.user_info)
				ser["uid"] = static_cast<std::uint64_t>(session.user_info->uid_);
			ser["verifyinfo"] = { { "tel", session.verify_telephone },  { "verify_cookie", session.verify_session_cookie ? verify_session_access::as_string(session.verify_session_cookie.value()) : std::string("") } };

			co_await mdbx_db.put(session_id, json_to_string(ser) );
		}

		awaitable<void> update_lifetime(std::string_view session_id, std::chrono::duration<int> lifetime)
		{
			// TODO, 更新　mdbx 里保存的 session
			co_return;
		}
	};

	persist_session::~persist_session() { impl().~persist_session_impl(); }

	persist_session::persist_session(std::filesystem::path persist_file)
	{
		static_assert(sizeof(obj_stor) >= sizeof(persist_session_impl));
		std::construct_at<persist_session_impl>(reinterpret_cast<persist_session_impl*>(obj_stor.data()), persist_file);
	}

	awaitable<bool> persist_session::exist(std::string_view key) const
	{
		co_return co_await impl().exist(key);
	}

	awaitable<client_session> persist_session::load(std::string_view session_id) const
	{
		co_return co_await impl().load(session_id);
	}

	awaitable<void> persist_session::save(const client_session& session, std::chrono::duration<int> lifetime)
	{
		co_return co_await impl().save(session.session_id, session, lifetime);
	}

	awaitable<void> persist_session::save(
		std::string_view session_id, const client_session& session, std::chrono::duration<int> lifetime)
	{
		co_return co_await impl().save(session_id, session, lifetime);
	}

	awaitable<void> persist_session::update_lifetime(
		std::string_view session_id, std::chrono::duration<int> lifetime)
	{
		co_return co_await impl().update_lifetime(session_id, lifetime);
	}

	const persist_session_impl& persist_session::impl() const
	{
		return *reinterpret_cast<const persist_session_impl*>(obj_stor.data());
	}

	persist_session_impl& persist_session::impl() { return *reinterpret_cast<persist_session_impl*>(obj_stor.data()); }

}
