
#include "cmall/conversion.hpp"
#include "cmall/misc.hpp"

inline namespace conversion
{
	using namespace boost::json;

	void tag_invoke(const value_from_tag&, value& jv, const cpp_numeric& u)
	{
		jv = to_string(u);
	}

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

	void tag_invoke(const value_from_tag&, value& jv, const cmall_product& p)
	{
		jv = {
			{ "id", p.id_ },
			{ "owner", p.owner_ },
			{ "name", p.name_ },
			{ "price", to_string(p.price_) },
			{ "currency", p.currency_ },
			{ "description", p.description_ },
			{ "state", p.state_ },
			{ "created_at", to_string(p.created_at_) },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_order& o)
	{
		jv = {
			{ "id", o.id_ },
			{ "oid", o.oid_ },
			{ "buyer", o.buyer_ },
			{ "price", to_string(o.price_) },
			{ "currency", o.currency_ },
			{ "pay_amount", to_string(o.pay_amount_) },
			{ "stage", o.stage_ },
			{ "payed_at", o.payed_at_.null() ? "" : to_string(o.payed_at_.get()) },
			{ "close_at", o.close_at_.null() ? "" : to_string(o.close_at_.get()) },
			{ "created_at", to_string(o.created_at_) },
			{ "ext", o.ext_.null() ? "" : o.ext_.get() },
		};
	}

	maybe_jsonrpc_request_t tag_invoke(const value_to_tag<maybe_jsonrpc_request_t>&, const value& jv)
	{
		if (!jv.is_object())
			return {};

		const auto& obj = jv.get_object();

		if (!obj.contains("method") || !obj.at("method").is_string())
			return {};

		auto method	 = value_to<std::string>(obj.at("method")); // will throw if not found

		jsonrpc_request_t req{ .method = method, .id = {}, .params = {} };
		if (obj.contains("id") && obj.at("id").is_string())
		{
			req.id = value_to<std::string>(obj.at("id"));
		}

		if (obj.contains("params") && (obj.at("params").is_object() || obj.at("params").is_array()))
		{
			req.params = obj.at("params");
		}

		return req;
	}
}