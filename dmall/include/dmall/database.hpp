//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <cstdint>
#include <odb/database.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/transaction.hxx>

#include <odb/pgsql/database.hxx>
#include <odb/pgsql/traits.hxx>
#include <vector>

#include "boost/asio/async_result.hpp"
#include "boost/asio/detail/config.hpp"
#include "db-odb.hxx"

#include "dmall/db.hpp"
#include "dmall/internal.hpp"

namespace dmall {

	struct db_config {
		std::string user_{ "postgres" };
		std::string password_{ "postgres" };
		std::string dbname_{ "dmall" };
		std::string host_{ "127.0.0.1" };
		unsigned int port_{ 5432 };
		int pool_{ 20 };
	};

	using db_result		= std::variant<bool, std::exception_ptr>;
	// using dns_records_t = std::vector<dmall_record>;

	class dmall_database {
		// c++11 noncopyable.
		dmall_database(const dmall_database&) = delete;
		dmall_database& operator=(const dmall_database&) = delete;

		// struct initiate_do_load_dmall_config {
		// 	template <typename Handler> void operator()(Handler&& handler, dmall_database* db, dmall_config* config) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), config]() mutable {
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
		// 							LOG_WARN << "initiate_do_load_dmall_config: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_load_dmall_config, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_add_dmall_config {
		// 	template <typename Handler> void operator()(Handler&& handler, dmall_database* db, dmall_config* config) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), config]() mutable {
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
		// 							LOG_WARN << "initiate_do_add_dmall_config: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(always_false<T>, "initiate_do_add_dmall_config, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_update_dmall_config {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, dmall_database* db, const dmall_config& config) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), config]() mutable {
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
		// 							LOG_WARN << "initiate_do_update_dmall_config: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_update_dmall_config, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_load_dmall_records {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, dmall_database* db, std::vector<dmall_record>* records, uint16_t type,
		// 		const std::string& name) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), records, type, name]() mutable {
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
		// 							LOG_WARN << "initiate_do_load_dmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_load_dmall_records, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_recursive_load_dmall_records {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, dmall_database* db, std::vector<dmall_record>* records, uint16_t type,
		// 		const std::string& name) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), records, type, name]() mutable {
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
		// 							LOG_WARN << "initiate_do_recursive_load_dmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_recursive_load_dmall_records, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_get_dmall_record {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, dmall_database* db, dmall_record* record, std::uint64_t rid) {
		// 		auto mdb = db->m_db;
		// 		if (!mdb) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), record, rid]() mutable {
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
		// 							LOG_WARN << "initiate_do_get_dmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_get_dmall_records, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_add_dmall_records {
		// 	template <typename Handler> void operator()(Handler&& handler, dmall_database* db, dmall_record* records) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), records]() mutable {
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
		// 							LOG_WARN << "initiate_do_add_dmall_records: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_add_dmall_records, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_update_dmall_record {
		// 	template <typename Handler>
		// 	void operator()(Handler&& handler, dmall_database* db, const dmall_record* record) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), record]() mutable {
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
		// 							LOG_WARN << "initiate_do_update_dmall_record: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_update_dmall_record, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

		// struct initiate_do_remove_dmall_record {
		// 	template <typename Handler> void operator()(Handler&& handler, dmall_database* db, std::uint64_t rid) {
		// 		auto mdb = db->m_db;
		// 		if (!db->m_db) {
		// 			auto ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 			handler(ec, false);
		// 			return;
		// 		}

		// 		db->m_io_context.post([this, db, mdb, handler = std::move(handler), rid]() mutable {
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
		// 							LOG_WARN << "initiate_do_remove_dmall_record: exception: " << e.what();
		// 						}
		// 						ec = boost::asio::error::make_error_code(boost::asio::error::no_recovery);
		// 					} else {
		// 						static_assert(
		// 							always_false<T>, "initiate_do_remove_dmall_record, non-exhaustive visitor!");
		// 					}

		// 					auto executor = boost::asio::get_associated_executor(handler);
		// 					boost::asio::post(executor, [ec, handler, result]() mutable { handler(ec, result); });
		// 				},
		// 				ret);
		// 		});
		// 	}
		// };

	public:
		dmall_database(const db_config& cfg, boost::asio::io_context& ioc);
		~dmall_database() = default;

	public:
		void shutdown();

		db_result load_config(dmall_config& config);
		db_result add_config(dmall_config& config);
		db_result update_config(const dmall_config& config);

		// db_result load_dns_records(std::vector<dmall_record>& records, uint16_t type, const std::string& name);
		// db_result load_dns_records_recursively(std::vector<dmall_record>& records, uint16_t type, const std::string& name);
		// db_result get_dns_record(std::uint64_t rid, dmall_record& record); // get by id
		// db_result add_dns_record(dmall_record& record);
		// db_result update_dns_record(const dmall_record& record);
		// db_result remove_dns_record(std::uint64_t rid);

	public:
		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_load_config(dmall_config& config, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_load_dmall_config{}, handler, this, &config);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_add_config(dmall_config& config, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_add_dmall_config{}, handler, this, &config);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_update_config(const dmall_config& config, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_update_dmall_config{}, handler, this, config);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_load_records(std::vector<dmall_record>& records, uint16_t type, const std::string& name,
		// 	BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_load_dmall_records{}, handler, this, &records, type, name);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_recursively_load_records(std::vector<dmall_record>& records, uint16_t type, const std::string& name,
		// 	BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_recursive_load_dmall_records{}, handler, this, &records, type, name);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_get_record(dmall_record& record, std::uint64_t rid, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_get_dmall_record{}, handler, this, &record, rid);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_add_record(dmall_record& records, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_add_dmall_records{}, handler, this, &records);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_update_record(const dmall_record& record, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_update_dmall_record{}, handler, this, &record);
		// }

		// template <typename Handler>
		// BOOST_ASIO_INITFN_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		// async_remove_record(std::uint64_t rid, BOOST_ASIO_MOVE_ARG(Handler) handler) {
		// 	return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>(
		// 		initiate_do_remove_dmall_record{}, handler, this, rid);
		// }

	private:
		template <typename T> db_result retry_database_op(T&& t) noexcept {
			if (!m_db) {
				try {
					throw std::bad_function_call();
				} catch (const std::exception&) { return std::current_exception(); }
			}

			using namespace std::chrono_literals;
			std::exception_ptr eptr;
			for (int retry_count(0); retry_count < db_max_retries; retry_count++) {
				try {
					return t();
				} catch (const odb::recoverable&) {
					eptr = std::current_exception();
					std::this_thread::sleep_for(5s);
					continue;
				} catch (const std::exception&) { return std::current_exception(); }
			}

			return eptr;
		}

	private:
		enum { db_max_retries = 25 };
		db_config m_config;
		boost::asio::io_context& m_io_context;
		boost::shared_ptr<odb::core::database> m_db;
	};
}
