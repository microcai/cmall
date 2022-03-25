
#include "stdafx.hpp"

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
}
