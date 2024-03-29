
#pragma once

#include <boost/json.hpp>
#include <optional>
#include <vector>

#include "cmall/db-odb.hxx"
#include "cmall/db.hpp"

namespace services{
	struct product;
	struct product_option_select;
	struct product_option;
	void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const product& p);
	void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const product_option_select& p);
	void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const product_option& p);
}

inline namespace conversion
{
	using namespace boost::json;
	using namespace services;

	// serialization
	void tag_invoke(const value_from_tag&, value& jv, const cmall_user& u);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_merchant& m);
	void tag_invoke(const value_from_tag&, value& jv, const Recipient& r);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_order& o);
	void tag_invoke(const value_from_tag&, value& jv, const goods_snapshot& g);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_cart& g);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_user_fav& g);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_apply_for_mechant& g);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_kuaidi_info& k);

	struct jsonrpc_request_t
	{
		std::string method;
		boost::json::value id;
		boost::json::object params; // only {}
	};
	using maybe_jsonrpc_request_t = std::optional<jsonrpc_request_t>;

	// deserialization
	maybe_jsonrpc_request_t tag_invoke(const value_to_tag<maybe_jsonrpc_request_t>&, const value&);
}

namespace boost::posix_time {
	boost::posix_time::ptime tag_invoke(const boost::json::value_to_tag<boost::posix_time::ptime>&, const boost::json::value&);
}
