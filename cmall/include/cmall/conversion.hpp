
#pragma once

#include <boost/json.hpp>
#include "cmall/db-odb.hxx"
#include "cmall/db.hpp"

inline namespace conversion
{
	void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const cmall_user& u);
	void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Recipient& u);
}
