
#include "cmall/conversion.hpp"
#include "cmall/misc.hpp"


namespace conversion
{

	void tag_invoke(const value_from_tag&, value& jv, const cmall_user& u)
	{
		jv = {
			{ "uid", u.uid_ },
			{ "name", u.name_ },
			{ "phone", u.active_phone },
			{ "verified", u.verified_ },
			{ "state", u.state_ },
			{ "recipients", u.recipients },
			{ "desc", u.desc_.null() ? "" : u.desc_.get() },
			{ "created_at", to_string(u.created_at_) },
		};
	}
	void tag_invoke(const value_from_tag&, value& jv, const Recipient& r)
	{
		jv = {
			{ "address", r.address },
			{ "is_default", r.as_default },
			{ "province", r.province.null() ? "" : r.province.get() },
			{ "city", r.city.null() ? "" : r.city.get() },
			{ "district", r.district.null() ? "" : r.district.get() },
			{ "specific_address", r.specific_address.null() ? "" : r.specific_address.get() },
		};
	}

	jsonrpc_request_t tag_invoke(const value_to_tag<jsonrpc_request_t>&, const value& jv) 
	{
		const auto& obj = jv.get_object();
		auto jsonrpc	= value_to<std::string>(obj.at("jsonrpc"));
		auto method	 = value_to<std::string>(obj.at("method")); // will throw if not found

		jsonrpc_request_t req{ .jsonrpc = jsonrpc, .method = method, .id = {}, .params = {} };
		if (obj.contains("id"))
		{
			req.id = value_to<std::string>(obj.at("id"));
		}

		if (obj.contains("params"))
		{
			req.params = obj.at("params");
		}

		return req;
	}
}