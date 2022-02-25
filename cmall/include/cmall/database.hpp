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
#include "boost/asio/use_awaitable.hpp"
#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/date_time/posix_time/ptime.hpp"
#include "boost/system/detail/error_code.hpp"
#include "cmall/db-odb.hxx"

#include "cmall/db.hpp"
#include "cmall/internal.hpp"
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

	using db_result = std::variant<bool, std::exception_ptr>;

	using extract_result_t = std::tuple<boost::system::error_code, bool, std::string>;

	template <typename Handler>
	void post_result(db_result v, Handler&& handler)
	{
		boost::system::error_code ec;
		bool ok = false;
		if (std::holds_alternative<bool>(v))
		{
			ok = std::get<0>(v);
		}
		else
		{
			ec = boost::asio::error::no_recovery;
		}
		auto excutor = boost::asio::get_associated_executor(handler);
		boost::asio::post(excutor, [handler = std::move(handler), ec, ok]() mutable { handler(ec, ok); });
	}

	template <typename T>
	concept SupportSoftDeletion = requires(T t)
	{
		{
			t.deleted_at_
			} -> std::convertible_to<odb::nullable<boost::posix_time::ptime>>;
	};
	template <typename T>
	concept SupportUpdateAt = requires(T t)
	{
		{
			t.updated_at_
			} -> std::convertible_to<boost::posix_time::ptime>;
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

		db_result load_config(cmall_config& config);
		db_result add_config(cmall_config& config);
		db_result update_config(const cmall_config& config);

		db_result load_user_by_phone(const ::std::string& phone, cmall_user& user);
		db_result load_all_products(std::vector<cmall_product>& products);

		template <typename T>
		db_result get(std::uint64_t id, T& ret)
		{
			if (!m_db)
				return false;

			return retry_database_op(
				[&, this]() mutable
				{
					bool ok = true;
					odb::transaction t(m_db->begin());
					auto record = m_db->load<T>(id);
					if (!record)
					{
						ok = false;
					}
					else
					{
						ret = *record;
						ok	= true;
					}
					t.commit();
					return ok;
				});
		}

		template <typename T>
		db_result add(T& value)
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

		template <SupportUpdateAt T>
		db_result update(T& value)
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

		template <typename T>
		db_result update(T& value)
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

		template <typename T>
		db_result hard_remove(std::uint64_t id)
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

		template <SupportSoftDeletion T>
		db_result soft_remove(std::uint64_t id)
		{
			if (!m_db)
				return false;

			return retry_database_op(
				[&, this]() mutable
				{
					bool ret = false;
					auto now = boost::posix_time::second_clock::local_time();
					odb::transaction t(m_db->begin());
					auto record = m_db->load<T>(id);
					if (!record)
					{
						ret = false;
					}
					else
					{
						record->deleted_at_ = now;
						m_db->update<T>(*record);
						ret = true;
					}
					t.commit();
					return ret;
				});
		}

		template <SupportSoftDeletion T>
		db_result soft_remove(T& value)
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
		boost::asio::awaitable<bool> async_load_all_products(std::vector<cmall_product>& products);


		template <typename T>
		boost::asio::awaitable<bool> async_load(std::uint64_t id, T& value)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, id, value](auto&& handler) mutable
				{
					boost::asio::post(thread_pool,
						[handler = std::move(handler), this, id, value]() mutable
						{
							auto ret = get<T>(id, value);
							post_result(ret, std::move(handler));
						});
				},
				boost::asio::use_awaitable);
		}

		template <typename T>
		boost::asio::awaitable<bool> async_add(T& value)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, &value](auto&& handler) mutable 
				{
					boost::asio::post(thread_pool, [handler = std::move(handler), this, &value]() mutable {
						auto ret = add<T>(value);
						post_result(ret, std::move(handler));
					});
				},
				boost::asio::use_awaitable);
		}

		template<SupportUpdateAt T>
		boost::asio::awaitable<bool> async_update(T& value)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, &value](auto&& handler) mutable 
				{
					boost::asio::post(thread_pool, [handler = std::move(handler), this, &value]() mutable {
						auto ret = update<T>(value);
						post_result(ret, std::move(handler));
					});
				},
				boost::asio::use_awaitable);
		}

		template<typename T>
		boost::asio::awaitable<bool> async_update(T& value)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, &value](auto&& handler) mutable 
				{
					boost::asio::post(thread_pool, [handler = std::move(handler), this, &value]() mutable {
						auto ret = update<T>(value);
						post_result(ret, std::move(handler));
					});
				},
				boost::asio::use_awaitable);
		}

		template<typename T>
		boost::asio::awaitable<bool> async_hard_remove(std::uint64_t id)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, id](auto&& handler) mutable 
				{
					boost::asio::post(thread_pool, [handler = std::move(handler), this, id]() mutable {
						auto ret = hard_remove<T>(id);
						post_result(ret, std::move(handler));
					});
				},
				boost::asio::use_awaitable);
		}

		template<SupportSoftDeletion T>
		boost::asio::awaitable<bool> async_soft_remove(std::uint64_t id)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, id](auto&& handler) mutable 
				{
					boost::asio::post(thread_pool, [handler = std::move(handler), this, id]() mutable {
						auto ret = soft_remove<T>(id);
						post_result(ret, std::move(handler));
					});
				},
				boost::asio::use_awaitable);
		}

		template<SupportSoftDeletion T>
		boost::asio::awaitable<bool> async_soft_remove(T& value)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, &value](auto&& handler) mutable 
				{
					boost::asio::post(thread_pool, [handler = std::move(handler), this, &value]() mutable {
						auto ret = soft_remove<T>(value);
						post_result(ret, std::move(handler));
					});
				},
				boost::asio::use_awaitable);
		}


	private:
		template <typename T>
		db_result retry_database_op(T&& t) noexcept
		{
			if (!m_db)
			{
				return std::make_exception_ptr(std::bad_function_call{});
			}

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
				catch (const std::exception&)
				{
					return std::current_exception();
				}
			}

			return eptr;
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
