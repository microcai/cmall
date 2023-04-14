
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

#include "services/persist_session.hpp"

#include "cmall/js_util.hpp"
#include "cmall/database.hpp"

static inline std::string json_to_string(const boost::json::value& jv, bool lf = true)
{
	if (lf) return boost::json::serialize(jv) + "\n";
	return boost::json::serialize(jv);
}

namespace services
{
	struct persist_session_impl
	{
		cmall::cmall_database& db;

		persist_session_impl(cmall::cmall_database& db)
			: db(db)
		{
		}

		~persist_session_impl() { }
		awaitable<bool> exist(std::string session_id) const
		{
			cmall_session s;
			co_return co_await db.async_load<cmall_session>(odb::query<cmall_session>::cache_key == session_id, s);
		}

		awaitable<client_session> load(std::string session_id) const
		{
			client_session cs;
			boost::json::object jv;

			cmall_session cached_session;

			co_await db.async_load<cmall_session>(odb::query<cmall_session>::cache_key == session_id , cached_session);

			jv = boost::json::parse(cached_session.cache_content, {}, { 64, false, false, true }).as_object();

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

			cs.sudo_mode = false;

			if (jv.contains("sudo_mode"))
			{
				cs.sudo_mode = jv.at("sudo_mode").as_bool();
			}

			if (cs.sudo_mode && jv.contains("original_user"))
			{
				administrators original_user;
				original_user.uid_ = jv.at("original_user").as_int64();
				cs.original_user = original_user;
			}

			co_return cs;
		}

		awaitable<void> save(std::string session_id, const client_session& session, std::chrono::duration<int> lifetime)
		{
			// TODO, 用　mdbx 保存 session

			boost::json::object ser;
			if (session.user_info)
				ser["uid"] = static_cast<std::uint64_t>(session.user_info->uid_);
			ser["verifyinfo"] = { { "tel", session.verify_telephone },  { "verify_cookie", session.verify_session_cookie ? verify_session_access::as_string(session.verify_session_cookie.value()) : std::string("") } };
			ser["sudo_mode"] =  session.sudo_mode;
			if (session.original_user)
				ser["original_user"] =  session.original_user->uid_;

			cmall_session s;
			s.cache_key = session_id;
			s.updated_at_ = boost::posix_time::second_clock::local_time();
			s.cache_content = json_to_string(ser);

			co_await db.async_upset(s);
		}

		awaitable<void> update_lifetime(std::string session_id)
		{
			co_await db.async_update<cmall_session>(odb::query<cmall_session>::cache_key == session_id, [](cmall_session s){
				s.updated_at_ = boost::posix_time::second_clock::local_time();
				return s;
			});
			co_return;
		}
	};

	persist_session::~persist_session() { impl().~persist_session_impl(); }

	persist_session::persist_session(cmall::cmall_database& db)
	{
		static_assert(sizeof(obj_stor) >= sizeof(persist_session_impl));
		std::construct_at<persist_session_impl>(reinterpret_cast<persist_session_impl*>(obj_stor.data()), db);
	}

	awaitable<bool> persist_session::exist(std::string key) const
	{
		co_return co_await impl().exist(key);
	}

	awaitable<client_session> persist_session::load(std::string session_id) const
	{
		co_return co_await impl().load(session_id);
	}

	awaitable<void> persist_session::save(const client_session& session, std::chrono::duration<int> lifetime)
	{
		co_return co_await impl().save(session.session_id, session, lifetime);
	}

	awaitable<void> persist_session::save(std::string session_id, const client_session& session, std::chrono::duration<int> lifetime)
	{
		co_return co_await impl().save(session_id, session, lifetime);
	}

	awaitable<void> persist_session::update_lifetime(std::string session_id)
	{
		co_return co_await impl().update_lifetime(session_id);
	}

	const persist_session_impl& persist_session::impl() const
	{
		return *reinterpret_cast<const persist_session_impl*>(obj_stor.data());
	}

	persist_session_impl& persist_session::impl() { return *reinterpret_cast<persist_session_impl*>(obj_stor.data()); }

}
