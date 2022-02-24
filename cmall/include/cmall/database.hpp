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

		template <typename T>
		struct initiate_do_load_by_id
		{
			template <typename Handler>
			void operator()(Handler&& handler, cmall_database* db, std::uint64_t id, T* value)
			{
				auto mdb = db->m_db;
				if (!mdb)
				{
					auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
					handler(ec, false);
					return;
				}

				boost::asio::post(db->m_io_context,
					[this, db, handler = std::move(handler), id, value]() mutable
					{
						auto ret = db->get<T>(id, *value);
						post_result(ret, std::move(handler));
					});
			}
		};

		template <typename T>
		struct initiate_do_add
		{
			template <typename Handler>
			void operator()(Handler&& handler, cmall_database* db, T* value)
			{
				auto mdb = db->m_db;
				if (!mdb)
				{
					auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
					handler(ec, false);
					return;
				}

				boost::asio::post(db->m_io_context,
					[db, handler = std::move(handler), value]() mutable
					{
						auto ret = db->add<T>(*value);
						post_result(ret, std::move(handler));
					});
			}
		};

		template <typename T>
		struct initiate_do_update
		{
			template <typename Handler>
			void operator()(Handler&& handler, cmall_database* db, T* value)
			{
				auto mdb = db->m_db;
				if (!mdb)
				{
					auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
					handler(ec, false);
					return;
				}

				boost::asio::post(db->m_io_context,
					[db, handler = std::move(handler), value]() mutable
					{
						auto ret = db->update<T>(*value);
						post_result(ret, std::move(handler));
					});
			}
		};

		template <SupportUpdateAt T>
		struct initiate_do_update<T>
		{
			template <typename Handler>
			void operator()(Handler&& handler, cmall_database* db, T* value)
			{
				auto mdb = db->m_db;
				if (!mdb)
				{
					auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
					handler(ec, false);
					return;
				}

				boost::asio::post(db->m_io_context,
					[db, handler = std::move(handler), value]() mutable
					{
						auto ret = db->update<T>(*value);
						post_result(ret, std::move(handler));
					});
			}
		};

		template <typename T>
		struct initiate_do_soft_remove
		{
			template <typename Handler>
			void operator()(Handler&& handler, cmall_database* db, T* value)
			{
				auto mdb = db->m_db;
				if (!mdb)
				{
					auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
					handler(ec, false);
					return;
				}

				boost::asio::post(db->m_io_context,
					[db, handler = std::move(handler), value]() mutable
					{
						auto ret = db->soft_remove<T>(*value);
						post_result(ret, std::move(handler));
					});
			}
		};

		template <typename T>
		struct initiate_do_soft_remove_by_id
		{
			template <typename Handler>
			void operator()(Handler&& handler, cmall_database* db, std::uint64_t id)
			{
				auto mdb = db->m_db;
				if (!mdb)
				{
					auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
					handler(ec, false);
					return;
				}

				boost::asio::post(db->m_io_context,
					[db, handler = std::move(handler), id]() mutable
					{
						auto ret = db->soft_remove<T>(id);
						post_result(ret, std::move(handler));
					});
			}
		};

		template <typename T>
		struct initiate_do_hard_remove
		{
			template <typename Handler>
			void operator()(Handler&& handler, cmall_database* db, std::uint64_t id)
			{
				auto mdb = db->m_db;
				if (!mdb)
				{
					auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
					handler(ec, false);
					return;
				}

				boost::asio::post(db->m_io_context,
					[db, handler = std::move(handler), id]() mutable
					{
						auto ret = db->hard_remove<T>(id);
						post_result(ret, std::move(handler));
					});
			}
		};

		// struct initiate_do_load_cmall_config {
		// 	template <typename Handler> void operator()(Handler&& handler, cmall_database* db, cmall_config* config) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), config]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->load_config(*config);
		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_load_cmall_config: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_load_cmall_config, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_add_cmall_config {
		// 	template <typename Handler> void operator()(Handler&& handler, cmall_database* db, cmall_config* config) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), config]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->add_config(*config);

		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_add_cmall_config: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(always_false<T>, "initiate_do_add_cmall_config, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_update_cmall_config {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, cmall_database* db, const cmall_config& config) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), config]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->update_config(config);

		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_update_cmall_config: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_update_cmall_config, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_load_cmall_records {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, cmall_database* db, std::vector<cmall_record>* records, uint16_t type,
		// 		const std::string& name) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), records, type, name]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->load_dns_records(*records, type, name);
		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_load_cmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_load_cmall_records, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_recursive_load_cmall_records {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, cmall_database* db, std::vector<cmall_record>* records, uint16_t type,
		// 		const std::string& name) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), records, type, name]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->load_dns_records_recursively(*records, type, name);
		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_recursive_load_cmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_recursive_load_cmall_records, non-exhaustive
		// visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_get_cmall_record {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, cmall_database* db, cmall_record* record, std::uint64_t rid) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), record, rid]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->get_dns_record(rid, *record);
		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_get_cmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_get_cmall_records, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_add_cmall_records {
		// 	template <typename Handler> void operator()(Handler&& handler, cmall_database* db, cmall_record* records) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), records]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->add_dns_record(*records);

		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_add_cmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_add_cmall_records, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_update_cmall_record {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, cmall_database* db, const cmall_record* record) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), record]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->update_dns_record(*record);

		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_update_cmall_record: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_update_cmall_record, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_remove_cmall_record {
		// 	template <typename Handler> void operator()(Handler&& handler, cmall_database* db, std::uint64_t rid) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		boost::asio::post(db->m_io_context, [this, db, mdb, handler = std::move(handler), rid]() mutable {
		// 			boost::system::error_code ec;
		// 			bool result = false;

		// 			auto ret = db->remove_dns_record(rid);

		// 			std::visit(
		// 				[&handler, &ec, &result](auto&& arg) mutable {
		// 					using T = std::decay_t<decltype(arg)>;
		// 					if constexpr (std::is_same_v<T, bool>) {
		// 						result = arg;
		// 					} else if constexpr (std::is_same_v<T, std::exception_ptr>) {
		// 						try {
		// 							std::rethrow_exception(arg);
		// 						} catch (const std::exception& e) {
		// 							LOG_WARN << "initiate_do_remove_cmall_record: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_remove_cmall_record, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

	public:
		cmall_database(const db_config& cfg, boost::asio::io_context& ioc);
		~cmall_database() = default;

	public:
		void shutdown();

		db_result load_config(cmall_config& config);
		db_result add_config(cmall_config& config);
		db_result update_config(const cmall_config& config);

		db_result load_user_by_phone(const ::std::string& phone, cmall_user& user);

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
						record->deleted_at = now;
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

		// db_result load_dns_records(std::vector<cmall_record>& records, uint16_t type, const std::string& name);
		// db_result load_dns_records_recursively(std::vector<cmall_record>& records, uint16_t type, const std::string&
		// name); db_result get_dns_record(std::uint64_t rid, cmall_record& record); // get by id db_result
		// add_dns_record(cmall_record& record); db_result update_dns_record(const cmall_record& record); db_result
		// remove_dns_record(std::uint64_t rid);

	public:
		boost::asio::awaitable<bool> async_load_user_by_phone(const std::string& phone, cmall_user& value);

		template <typename T>
		boost::asio::awaitable<bool> async_load(std::uint64_t id, T& value)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, id, value](auto&& handler) mutable
				{
					boost::asio::post(m_io_context,
						[handler = std::move(handler), this, id, value]() mutable
						{
							auto ret = get<T>(id, value);
							post_result(ret, std::move(handler));
						});
				},
				boost::asio::use_awaitable);
		}

		template <typename Handler, typename T>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		async_add(T& value, BOOST_ASIO_MOVE_ARG(Handler) handler)
		{
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
				initiate_do_add<T>{}, handler, this, &value);
		}

		template <typename T>
		boost::asio::awaitable<bool> async_add(T& value)
		{
			return boost::asio::async_initiate<decltype(boost::asio::use_awaitable),
				void(boost::system::error_code, bool)>(
				[this, value](auto&& handler) mutable 
				{
					boost::asio::post(m_io_context, [handler = std::move(handler), this, value]() mutable {
						auto ret = add<T>(value);
						post_result(ret, std::move(handler));
					});
				},
				boost::asio::use_awaitable);
		}

		template <typename Handler, SupportUpdateAt T>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		async_update(T& value, BOOST_ASIO_MOVE_ARG(Handler) handler)
		{
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
				initiate_do_update<T>{}, handler, this, &value);
		}
		template <typename Handler, typename T>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		async_update(T& value, BOOST_ASIO_MOVE_ARG(Handler) handler)
		{
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
				initiate_do_update<T>{}, handler, this, &value);
		}
		template <typename T, typename Handler>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		async_hard_remove(std::uint64_t id, BOOST_ASIO_MOVE_ARG(Handler) handler)
		{
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
				initiate_do_hard_remove<T>{}, handler, this, id);
		}
		template <typename T, typename Handler>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		async_soft_remove(T& value, BOOST_ASIO_MOVE_ARG(Handler) handler)
		{
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
				initiate_do_soft_remove<T>{}, handler, this, &value);
		}
		template <typename T, typename Handler>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		async_soft_remove(std::uint64_t id, BOOST_ASIO_MOVE_ARG(Handler) handler)
		{
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
				initiate_do_soft_remove_by_id<T>{}, handler, this, id);
		}

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_load_config(cmall_config& config, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_load_cmall_config{}, handler, this, &config);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_add_config(cmall_config& config, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_add_cmall_config{}, handler, this, &config);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_update_config(const cmall_config& config, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_update_cmall_config{}, handler, this, config);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_load_records(std::vector<cmall_record>& records, uint16_t type, const std::string& name,
		// 	BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_load_cmall_records{}, handler, this, &records, type, name);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_recursively_load_records(std::vector<cmall_record>& records, uint16_t type, const std::string& name,
		// 	BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_recursive_load_cmall_records{}, handler, this, &records, type, name);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_get_record(cmall_record& record, std::uint64_t rid, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_get_cmall_record{}, handler, this, &record, rid);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_add_record(cmall_record& records, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_add_cmall_records{}, handler, this, &records);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_update_record(const cmall_record& record, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_update_cmall_record{}, handler, this, &record);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_remove_record(std::uint64_t rid, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_remove_cmall_record{}, handler, this, rid);
		// }

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
		boost::asio::io_context& m_io_context;
		boost::shared_ptr<odb::core::database> m_db;
	};
}
