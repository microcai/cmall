
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

	db_result cmall_database::load_config(cmall_config& config)
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

	db_result cmall_database::add_config(cmall_config& config)
	{
		if (!m_db)
			return false;

		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			m_db->persist(config);
			t.commit();
			return true;
		});
	}

	db_result cmall_database::update_config(const cmall_config& config)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			m_db->update(config);
			t.commit();
			return true;
		});
	}

	db_result cmall_database::load_user_by_phone(const std::string& phone, cmall_user& user)
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

	db_result cmall_database::load_all_products(std::vector<cmall_product>& products)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			t.tracer(odb::stderr_tracer);
			auto r(m_db->query<cmall_product>(odb::query<cmall_product>::deleted_at.is_null()));

			for (auto i : r)
				products.push_back(i);
			t.commit();

			return true;
		});
	}

	db_result cmall_database::load_all_products_by_merchant(std::vector<cmall_product>& products, long merchant_id)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			auto r(m_db->query<cmall_product>(odb::query<cmall_product>::owner == merchant_id && odb::query<cmall_product>::deleted_at.is_null()));

			for (auto i : r)
				products.push_back(i);
			t.commit();

			return true;
		});
	}

	db_result cmall_database::load_all_user_orders(std::vector<cmall_order>& orders, std::uint64_t uid)
	{
		return retry_database_op([&, this]() mutable
		{
			odb::transaction t(m_db->begin());
			using query = odb::query<cmall_order>;
			auto r = m_db->query<cmall_order>(
				(query::buyer == uid && query::deleted_at.is_null()) + " order by " + query::created_at + " desc"
			);

			for (auto i : r)
				orders.push_back(i);
			t.commit();

			LOG_DBG << "load_all_user_orders(" << uid << ") retrived " << orders.size() << " orders";

			return true;
		});
	}

	boost::asio::awaitable<bool> cmall_database::async_load_user_by_phone(const std::string& phone, cmall_user& value)
	{
		return boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void(boost::system::error_code, bool)>(
			[&, this](auto&& handler) mutable
			{
				boost::asio::post(thread_pool,
				[&, this, handler = std::move(handler)]() mutable
				{
					auto ret = load_user_by_phone(phone, value);
					post_result(ret, std::move(handler));
				});
			}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<bool> cmall_database::async_load_all_products(std::vector<cmall_product>& products)
	{
		return boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void(boost::system::error_code, bool)>(
			[&, this](auto&& handler) mutable
			{
				boost::asio::post(thread_pool,
				[&, this, handler = std::move(handler)]() mutable
				{
					auto ret = load_all_products(products);
					post_result(ret, std::move(handler));
				});
			}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<bool> cmall_database::async_load_all_products_by_merchant(std::vector<cmall_product>& products, long merchant_id)
	{
		return boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void(boost::system::error_code, bool)>(
			[&, this](auto&& handler) mutable
			{
				boost::asio::post(thread_pool,
				[&, this, handler = std::move(handler)]() mutable
				{
					auto ret = load_all_products_by_merchant(products, merchant_id);
					post_result(ret, std::move(handler));
				});
			}, boost::asio::use_awaitable);
	}

	boost::asio::awaitable<bool> cmall_database::async_load_all_user_orders(std::vector<cmall_order>& orders, std::uint64_t uid)
	{
		return boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void(boost::system::error_code, bool)>(
			[&, uid, this](auto&& handler) mutable
			{
				boost::asio::post(thread_pool,
				[&, this, handler = std::move(handler)]() mutable
				{
					auto ret = load_all_user_orders(orders, uid);
					post_result(ret, std::move(handler));
				});
			}, boost::asio::use_awaitable);
	}

}
