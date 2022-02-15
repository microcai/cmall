#include "cmall/database.hpp"
#include "db.hpp"
#include "odb/transaction.hxx"
#include "cmall/logging.hpp"

namespace cmall {
	cmall_database::cmall_database(const db_config& cfg, boost::asio::io_context& ioc)
		: m_config(cfg)
		, m_io_context(ioc)
	{
		if (m_config.host_.empty() ||
			m_config.dbname_.empty() ||
			m_config.user_.empty())
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

	// db_result cmall_database::load_dns_records(std::vector<cmall_record> &records, uint16_t type, const std::string& name)
	// {
	// 	if (!m_db)
	// 		return false;

	// 	return retry_database_op([&, this]() mutable
	// 	{
	// 		odb::transaction t(m_db->begin());

	// 		using query = odb::query<cmall_record>;

	// 		auto result(m_db->query<cmall_record>((query::type == type && query::name == name) + " order by created_at desc"));
	// 		if (result.empty()) {
	// 			t.commit();
	// 			return false;
	// 		}

	// 		for (auto& r : result)
	// 			records.push_back(r);
	// 		t.commit();

	// 		return true;
	// 	});
	// }

	// db_result cmall_database::load_dns_records_recursively(std::vector<cmall_record> &records, uint16_t type, const std::string& name)
	// {
	// 	if (!m_db)
	// 		return false;

	// 	return retry_database_op([&, this]() mutable
	// 	{
	// 		odb::transaction t(m_db->begin());

	// 		using query = odb::query<cmall_record>;

	// 		if (type == to_underlying(dns_type::CNAME)) { // directly querying CNAME
	// 			auto result(m_db->query<cmall_record>((query::type == type && query::name == name) + " order by created_at desc"));
	// 			if (result.empty()) {
	// 				t.commit();
	// 				return false;
	// 			}

	// 			for (auto& r : result)
	// 				records.push_back(r);
	// 			t.commit();

	// 			return true;
	// 		} else {
	// 			std::deque<std::string> qn;
	// 			bool found = false;

	// 			auto result(m_db->query<cmall_record>(query::name == name));
	// 			if (result.empty()) {
	// 				t.commit();
	// 				return false;
	// 			}

	// 			for (auto& r : result) {
	// 				records.push_back(r);
	// 				if (r.type_ == type) found = true;
	// 				if (r.type_ == to_underlying(dns_type::CNAME)) {
	// 					qn.push_back(r.content_);
	// 				}
	// 			}
	// 			if (found) {
	// 				t.commit();
	// 				return true;
	// 			}

	// 			while (qn.size() > 0) {
	// 				auto const &it = qn[0];
	// 				auto result(m_db->query<cmall_record>(query::name == it));
	// 				if (!result.empty()) {
	// 					for (auto& r : result) {
	// 						records.push_back(r);
	// 						if (r.type_ == type) found = true;
	// 						if (r.type_ == to_underlying(dns_type::CNAME)) {
	// 							qn.push_back(r.content_);
	// 						}
	// 					}
	// 				}

	// 				qn.pop_front();
	// 			}

	// 			t.commit();
	// 			return found;
	// 		}

	// 	});
	// }

	// db_result cmall_database::get_dns_record(std::uint64_t rid, cmall_record &record)
	// {
	// 	if (!m_db)
	// 		return false;

	// 	return retry_database_op([&, this]()
	// 	{
	// 		auto ret = false;
	// 		odb::transaction t(m_db->begin());
	// 		ret = m_db->find<cmall_record>(rid, record);
	// 		t.commit();
	// 		return ret;
	// 	});
	// }

	// db_result cmall_database::add_dns_record(cmall_record &record)
	// {
	// 	if (!m_db)
	// 		return false;

	// 	return retry_database_op([&, this]() mutable
	// 	{
	// 		odb::transaction t(m_db->begin());
	// 		m_db->persist(record);
	// 		t.commit();
	// 		return true;
	// 	});
	// }

	// db_result cmall_database::update_dns_record(const cmall_record &record)
	// {
	// 	if (!m_db)
	// 		return false;

	// 	return retry_database_op([&, this]() 
	// 	{
	// 		odb::transaction t(m_db->begin());
	// 		m_db->update(record);
	// 		t.commit();
	// 		return true;
	// 	});
	// }

	// db_result cmall_database::remove_dns_record(std::uint64_t rid)
	// {
	// 	if (!m_db)
	// 		return false;

	// 	return retry_database_op([&, this]()
	// 	{
	// 		odb::transaction t(m_db->begin());
	// 		m_db->erase<cmall_record>(rid);
	// 		t.commit();
	// 		return true;
	// 	});
	// }
}
