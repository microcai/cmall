
#include "utils/logging.hpp"

#include "cmall/database.hpp"
#include "cmall/db.hpp"
#include "odb/transaction.hxx"

namespace cmall {
	cmall_database::cmall_database(const db_config& cfg)
		: m_config(cfg)
		, thread_pool(cfg.pool_)
	{
		if (m_config.dbname_.empty())
		{
			LOG_WARN << "Warning, Database not config!";
			return;
		}

		std::unique_ptr<odb::pgsql::connection_factory>
			pool(new odb::pgsql::connection_pool_factory(cfg.pool_, cfg.pool_ / 2));

		m_db = boost::make_shared<odb::pgsql::database>(m_config.user_,
			m_config.password_, m_config.dbname_, m_config.host_, m_config.port_,
			"application_name=cmall", std::move(pool));

		odb::transaction t(m_db->begin());
		if (m_db->schema_version() == 0)
		{
			t.commit();
			t.reset(m_db->begin());
			odb::schema_catalog::create_schema(*m_db, "", true);
			t.commit();
		}
		else
		{
			t.commit();
			t.reset(m_db->begin());
			odb::schema_catalog::migrate(*m_db);
			t.commit();
		}
	}

	void cmall_database::shutdown()
	{
		thread_pool.join();
		m_db.reset();
	}

	bool cmall_database::load_config(cmall_config& config)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			auto r(m_db->query<cmall_config>());
			if (r.empty())
			{
				t.commit();
				return false;
			}

			config = *r.begin();
			t.commit();

			return true;
		});
	}

	bool cmall_database::add_config(cmall_config& config)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			m_db->persist(config);
			t.commit();
			return true;
		});
	}

	bool cmall_database::update_config(const cmall_config& config)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			m_db->update(config);
			t.commit();
			return true;
		});
	}

	bool cmall_database::load_user_by_phone(const std::string& phone, cmall_user& user)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			using query = odb::query<cmall_user>;
			auto r(m_db->query<cmall_user>(query::active_phone == phone));
			if (r.empty())
			{
				t.commit();
				return false;
			}

			user = *r.begin();
			t.commit();

			return true;
		});
	}

	bool cmall_database::load_all_user_orders(std::vector<cmall_order>& orders, std::uint64_t uid, int page, int page_size)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			using query = odb::query<cmall_order>;
			auto r = m_db->query<cmall_order>(
				(query::buyer == uid && query::deleted_at.is_null()) + " order by " + query::created_at + " desc limit " + std::to_string(page_size) + " offset " + std::to_string(page * page_size)
			);

			for (auto i : r)
				orders.push_back(i);
			t.commit();

			LOG_DBG << "load_all_user_orders(" << uid << ") retrived " << orders.size() << " orders";

			return true;
		});
	}

	bool cmall_database::load_all_user_cart(std::vector<cmall_cart>& items, std::uint64_t uid, int page, int page_size)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			using query = odb::query<cmall_cart>;
			auto r = m_db->query<cmall_cart>(
				(query::uid == uid) + " order by " + query::created_at + " desc limit " + std::to_string(page_size) + " offset " + std::to_string(page * page_size)
			);

			for (auto i : r)
				items.push_back(i);
			t.commit();

			LOG_DBG << "load_all_user_cart(" << uid << ") retrived " << items.size() << " orders";

			return true;
		});
	}

	bool cmall_database::load_order(cmall_order& order, std::string orderid)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			using query = odb::query<cmall_order>;
			auto r = m_db->query<cmall_order>(query::oid ==  orderid);

			if (r.empty())
			{
				t.commit();
				LOG_DBG << "load_order(" << orderid << ") not found";
				return false;
			}

			for (auto i : r)
				order = i;
			t.commit();

			LOG_DBG << "load_order(" << orderid << ") retrived";

			return true;
		});
	}

	bool cmall_database::load_all_merchant(std::vector<cmall_merchant>& merchants)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			auto r = m_db->query<cmall_merchant>(odb::query<cmall_merchant>::deleted_at.is_null() );

			if (r.empty())
			{
				t.commit();
				return false;
			}

			for (auto i : r)
				merchants.push_back(i);
			t.commit();

			return true;
		});
	}


	boost::asio::awaitable<bool> cmall_database::async_load_user_by_phone(const std::string& phone, cmall_user& value)
	{
		return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
		{
			co_return load_user_by_phone(phone, value);
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<bool> cmall_database::async_load_all_user_orders(std::vector<cmall_order>& orders, std::uint64_t uid, int page, int page_size)
	{
		return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
		{
			co_return load_all_user_orders(orders, uid, page, page_size);
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<bool> cmall_database::async_load_order(cmall_order& order, std::string orderid)
	{
		return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
		{
			co_return load_order(order, orderid);
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<bool> cmall_database::async_load_all_merchant(std::vector<cmall_merchant>& merchants)
	{
		return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
		{
			co_return load_all_merchant(merchants);
		}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<bool> cmall_database::async_load_all_user_cart(std::vector<cmall_cart>& items, std::uint64_t uid, int page, int page_size)
	{
		return boost::asio::co_spawn(thread_pool, [&, this]()mutable -> boost::asio::awaitable<bool>
		{
			co_return load_all_user_cart(items, uid, page, page_size);
		}, boost::asio::use_awaitable);
	}
}
