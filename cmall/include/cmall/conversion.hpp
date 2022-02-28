
#pragma once

#include <boost/json.hpp>
#include <optional>
#include "cmall/db-odb.hxx"
#include "cmall/db.hpp"

inline namespace conversion
{
	using namespace boost::json;
	// serialization
	void tag_invoke(const value_from_tag&, value& jv, const cmall_user& u);
	void tag_invoke(const value_from_tag&, value& jv, const Recipient& r);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_product& p);
	void tag_invoke(const value_from_tag&, value& jv, const cmall_order& o);

	struct jsonrpc_request_t
	{
		std::string method;
		boost::json::value id;
		std::optional<value> params; // must be [] or {}
	};
	using maybe_jsonrpc_request_t = std::optional<jsonrpc_request_t>;

	// deserialization
	maybe_jsonrpc_request_t tag_invoke(const value_to_tag<maybe_jsonrpc_request_t>&, const value&);
}
