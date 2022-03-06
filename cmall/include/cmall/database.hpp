//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <cstdint>
#include <exception>
#include <odb/database.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/transaction.hxx>

#include <odb/pgsql/database.hxx>
#include <odb/pgsql/traits.hxx>
#include <tuple>
#include <variant>
#include <vector>

#include <boost/asio.hpp>

#include "boost/asio/async_result.hpp"
#include "boost/asio/awaitable.hpp"
#include "boost/asio/co_spawn.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/date_time/posix_time/ptime.hpp"
#include "boost/system/detail/error_code.hpp"
#include "boost/system/system_error.hpp"
#include "cmall/db-odb.hxx"

#include "cmall/db.hpp"
#include "cmall/error_code.hpp"
#include "cmall/internal.hpp"
#include "odb/forward.hxx"
#include "odb/traits.hxx"
#include "utils/logging.hpp"

namespace cmall
{
	struct db_config
	{
		std::string user_{ "postgres" };
		std::string password_{ "postgres" };
		std::string dbname_{ "cmall" };
		std::string host_{ "127.0.0.1" };
		unsigned int port_{ 5432 };
		int pool_{ 20 };
	};

	template <typename T>
	concept SupportSoftDeletion = requires(T t)
	{
		{ t.deleted_at_ } -> std::convertible_to<odb::nullable<boost::posix_time::ptime>>;
	};
	template <typename T>
	concept SupportUpdateAt = requires(T t)
	{
		{ t.updated_at_ } -> std::convertible_to<boost::posix_time::ptime>;
	};

	class cmall_database
	{
		// c++11 noncopyable.
		cmall_database(const cmall_database&) = delete;
		cmall_database& operator=(const cmall_database&) = delete;

	public:
		cmall_database(const db_config& cfg);
		~cmall_database() = default;

	public:
		void shutdown();

		bool load_config(cmall_config& config);
		bool add_config(cmall_config& config);
		bool update_config(const cmall_config& config);

		bool load_user_by_phone(const ::std::string& phone, cmall_user& user);
		bool load_all_user_orders(std::vector<cmall_order>& orders, std::uint64_t uid, int page, int page_size);
		bool load_order(cmall_order& order, std::string orderid);

		bool load_all_merchant(std::vector<cmall_merchant>& merchants);
		bool load_all_user_cart(std::vector<cmall_cart>& items, std::uint64_t uid, int page, int page_size);

		template <typename T>
		bool get(std::uint64_t id, T& ret)
		{
			return retry_database_op(
				[&, this]() mutable
				{
					odb::transaction t(m_db->begin());
					t.tracer(odb::stderr_full_tracer);
					bool ok = m_db->find<T>(id, ret);
					t.commit();
					return ok;
				});
		}

		template <typename T>
		bool add(T& value)
		{
			if (!m_db)
				return false;

			return retry_database_op(
				[&, this]() mutable
				{
					odb::transaction t(m_db->begin());
					m_db->persist(value);
					t.commit();
					return true;
				});
		}

		template <typename T> requires SupportUpdateAt<T>
		bool update(T& value)
		{
			if (!m_db)
				return false;

			LOG_DBG << "call update with SupportUpdateAt";
			return retry_database_op(
				[&, this]() mutable
				{
					auto now		  = boost::posix_time::second_clock::local_time();
					value.updated_at_ = now;
					odb::transaction t(m_db->begin());
					m_db->update(value);
					t.commit();
					return true;
				});
		}

		template <typename T, typename UPDATER> requires SupportUpdateAt<T>
		bool update(const typename odb::object_traits<T>::id_type id, UPDATER&& updater)
		{
			if (!m_db)
				return false;

			LOG_DBG << "call update with SupportUpdateAt";
			return retry_database_op(
				[&, this]() mutable
				{
					odb::transaction t(m_db->begin());
					T old_value;
					m_db->load<T>(id, old_value);
					T new_value = updater(std::move(old_value));
					auto now		  = boost::posix_time::second_clock::local_time();
					new_value.updated_at_ = now;
					m_db->update(new_value);
					t.commit();
					return true;
				});
		}

		template <typename T>
		bool update(T& value)
		{
			if (!m_db)
				return false;

			LOG_DBG << "call update with T";
			return retry_database_op(
				[&, this]() mutable
				{
					odb::transaction t(m_db->begin());
					m_db->update(value);
					t.commit();
					return true;
				});
		}

		template <typename T, typename UPDATER> requires (!SupportUpdateAt<T>)
		bool update(const typename odb::object_traits<T>::id_type id, UPDATER&& updater)
		{
			if (!m_db)
				return false;

			LOG_DBG << "call update with SupportUpdateAt";
			return retry_database_op(
				[&, this]() mutable
				{
					odb::transaction t(m_db->begin());
					T old_value;
					m_db->load<T>(id, old_value);
					T new_value = updater(std::move(old_value));
					m_db->update(new_value);
					t.commit();
					return true;
				});
		}

		template <typename T>
		bool hard_remove(std::uint64_t id)
		{
			if (!m_db)
				return false;

			return retry_database_op(
				[&, this]() mutable
				{
					odb::transaction t(m_db->begin());
					m_db->erase<T>(id);
					t.commit();
					return true;
				});
		}

		template <typename T> requires SupportSoftDeletion<T>
		bool soft_remove(std::uint64_t id)
		{
			if (!m_db)
				return false;

			return retry_database_op(
				[&, this]() mutable
				{
					bool ret = false;
					auto now = boost::posix_time::second_clock::local_time();
					odb::transaction t(m_db->begin());
					T record;
					ret = m_db->find<T>(id, record);
					if (ret)
					{
						record->deleted_at_ = now;
						m_db->update<T>(record);
					}
					t.commit();
					return ret;
				});
		}

		template <typename T> requires SupportSoftDeletion<T>
		bool soft_remove(T& value)
		{
			if (!m_db)
				return false;

			return retry_database_op(
				[&, this]() mutable
				{
					auto now		  = boost::posix_time::second_clock::local_time();
					value.deleted_at_ = now;
					odb::transaction t(m_db->begin());
					m_db->update<T>(value);
					t.commit();
					return true;
				});
		}

	public:
		boost::asio::awaitable<bool> async_load_user_by_phone(const std::string& phone, cmall_user& value);
		boost::asio::awaitable<bool> async_load_all_user_orders(std::vector<cmall_order>& orders, std::uint64_t uid, int page, int page_size);
		boost::asio::awaitable<bool> async_load_order(cmall_order& orders, std::string orderid);
		boost::asio::awaitable<bool> async_load_all_merchant(std::vector<cmall_merchant>&);
		boost::asio::awaitable<bool> async_load_all_user_cart(std::vector<cmall_cart>& items, std::uint64_t uid, int page, int page_size);

		template <typename T>
		boost::asio::awaitable<bool> async_load(std::uint64_t id, T& value)
		{
			return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
			{
				co_return get<T>(id, value);
			}, boost::asio::use_awaitable);
		}

		template <typename T>
		boost::asio::awaitable<bool> async_add(T& value)
		{
			return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
			{
				co_return add<T>(value);
			}, boost::asio::use_awaitable);
		}

		template<typename T>
		boost::asio::awaitable<bool> async_update(T& value)
		{
			return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
			{
				co_return update<T>(value);
			}, boost::asio::use_awaitable);
		}

		template<typename T, typename UPDATER>
		boost::asio::awaitable<bool> async_update(const typename odb::object_traits<T>::id_type id, UPDATER && updater)
		{
			return boost::asio::co_spawn(thread_pool, [&, id, this]()mutable -> boost::asio::awaitable<bool>
			{
				co_return update<T>(id, std::forward<UPDATER>(updater));
			}, boost::asio::use_awaitable);
		}

		template<typename T>
		boost::asio::awaitable<bool> async_hard_remove(std::uint64_t id)
		{
			return boost::asio::co_spawn(thread_pool, [id, this]()mutable -> boost::asio::awaitable<bool>
			{
				co_return hard_remove<T>(id);
			}, boost::asio::use_awaitable);
		}

		template<SupportSoftDeletion T>
		boost::asio::awaitable<bool> async_soft_remove(std::uint64_t id)
		{
			return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
			{
				co_return soft_remove<T>(id);
			}, boost::asio::use_awaitable);
		}

		template<SupportSoftDeletion T>
		boost::asio::awaitable<bool> async_soft_remove(T& value)
		{
			return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
			{
				co_return soft_remove<T>(value);
			}, boost::asio::use_awaitable);
		}

	private:
		template <typename T>
		bool retry_database_op(T&& t)
		{
			using namespace std::chrono_literals;
			std::exception_ptr eptr;
			for (int retry_count(0); retry_count < db_max_retries; retry_count++)
			{
				try
				{
					return t();
				}
				catch (const odb::recoverable&)
				{
					eptr = std::current_exception();
					std::this_thread::sleep_for(5s);
					continue;
				}
				catch (const std::exception& e)
				{
					LOG_ERR << "db operation error:" << e.what();
					throw boost::system::system_error(cmall::error::internal_server_error);
				}
			}
			if (eptr)
				throw boost::system::system_error(cmall::error::internal_server_error);

			return false;
		}

	private:
		enum
		{
			db_max_retries = 25
		};
		db_config m_config;
		boost::asio::thread_pool thread_pool;
		boost::shared_ptr<odb::core::database> m_db;
	};
}
