
#include <boost/json.hpp>

#include "cmall/internal.hpp"
#include "cmall/misc.hpp"
#include <services/repo_products.hpp>

#include "cmall/conversion.hpp"

void services::tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const services::product& p)
{
	jv = {
		{ "goods_id", p.product_id },
		{ "merchant_id", p.merchant_id },
		{ "name", p.product_title },
		{ "price", p.product_price },
		{ "description", p.product_description },
		{ "pictures", p.pics },
	};
}

inline namespace conversion
{
	using namespace boost::json;

	void tag_invoke(const value_from_tag&, value& jv, const cpp_numeric& u) { jv = to_string(u); }

	void tag_invoke(const value_from_tag&, value& jv, const cmall_user& u)
	{
		jv = {
			{ "uid", u.uid_ },
			{ "name", u.name_ },
			{ "phone", u.active_phone },
			{ "verified", u.verified_ },
			{ "state", u.state_ },
			{ "desc", u.desc_.null() ? "" : u.desc_.get() },
			{ "created_at", ::to_string(u.created_at_) },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_merchant& m)
	{
		jv = {
			{ "uid", m.uid_ },
			{ "name", m.name_ },
			{ "state", m.state_ },
			{ "desc", m.desc_.null() ? "" : m.desc_.get() },
			{ "created_at", ::to_string(m.created_at_) },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const Recipient& r)
	{
		jv = {
			{ "address", r.address },
			{ "telephone", r.telephone },
			{ "name", r.name },
			{ "is_default", r.as_default },
			{ "province", r.province.null() ? "" : r.province.get() },
			{ "city", r.city.null() ? "" : r.city.get() },
			{ "district", r.district.null() ? "" : r.district.get() },
			{ "specific_address", r.specific_address.null() ? "" : r.specific_address.get() },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_order& o)
	{
		jv = {
			{ "orderid", o.oid_ },
			{ "buyer", o.buyer_ },
			{ "price", ::to_string(o.price_) },
			{ "pay_amount", ::to_string(o.pay_amount_) },
			{ "stage", o.stage_ },
			{ "payed_at", o.payed_at_.null() ? "" : ::to_string(o.payed_at_.get()) },
			{ "close_at", o.close_at_.null() ? "" : ::to_string(o.close_at_.get()) },
			{ "created_at", ::to_string(o.created_at_) },
			{ "bought_goods", o.bought_goods },
			{ "recipients", o.recipient },
			{ "git_version", o.snap_git_version },
			{ "kuaidiinfo", value_from(o.kuaidi) },
			{ "ext", o.ext_.null() ? "" : o.ext_.get() },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const goods_snapshot& g)
	{
		jv = {
			{ "merchant_id", g.merchant_id },
			{ "goods_id", g.goods_id },
			{ "name", g.name_ },
			{ "price", ::to_string(g.price_) },
			{ "description", g.description_ },
			{ "git_version", g.good_version_git },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_cart& c)
	{
		jv = {
			{ "item_id", c.id_ },
			{ "merchant_id", c.merchant_id_ },
			{ "goods_id", c.goods_id_ },
			{ "count", c.count_ },
			{ "created_at", to_string(c.created_at_) },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_apply_for_mechant& a)
	{
		jv = {
			{ "apply_id", a.id_ },
			{ "applicant_user_id", a.applicant_->uid_ },
			{ "approved", a.approved_ },
			{ "created_at", to_string(a.created_at_) },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_kuaidi_info& k)
	{
		jv = {
			{ "kuaidihao", k.kuaidihao },
			{ "kuaidigongsi", k.kuaidigongsi },
		};
	}

	maybe_jsonrpc_request_t tag_invoke(const value_to_tag<maybe_jsonrpc_request_t>&, const value& jv)
	{
		if (!jv.is_object())
			return {};

		const auto& obj = jv.get_object();

		if (!obj.contains("method") || !obj.at("method").is_string())
			return {};

		auto method = value_to<std::string>(obj.at("method"));

		jsonrpc_request_t req{ .method = method, .id = {}, .params = {} };
		if (obj.contains("id"))
		{
			req.id = obj.at("id");
		}

		if (obj.contains("params") && obj.at("params").is_object())
		{
			req.params = obj.at("params").as_object();
		}

		return req;
	}
}
