#include "stdafx.hpp"

#include "cmall/internal.hpp"
#include "cmall/misc.hpp"
#include "services/merchant_git_repo.hpp"

#include "cmall/conversion.hpp"

void services::tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const services::product& p)
{
	jv = {
		{ "goods_id", p.product_id },
		{ "merchant_id", p.merchant_id },
		{ "merchant_name", p.merchant_name },
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
		boost::json::value wx_submerchant_id{nullptr};

		if (!m.exinfo_wx_mchid.null())
			wx_submerchant_id = boost::json::value(m.exinfo_wx_mchid.get());

		jv = {
			{ "uid", m.uid_ },
			{ "name", m.name_ },
			{ "state", to_underlying(m.state_) },
			{ "desc", m.desc_.null() ? "" : m.desc_.get() },
			{ "created_at", ::to_string(m.created_at_) },
			{ "wx_submerchant_id",  wx_submerchant_id },
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
			{ "merchant_name", c.merchant_name_ },
			{ "goods_id", c.goods_id_ },
			{ "count", c.count_ },
			{ "created_at", to_string(c.created_at_) },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_user_fav& c)
	{
		jv = {
			{ "merchant_id", c.merchant_id_ },
			{ "merchant_name", c.merchant_name_ },
			{ "created_at", to_string(c.created_at_) },
		};
	}

	void tag_invoke(const value_from_tag&, value& jv, const cmall_apply_for_mechant& a)
	{
		jv = {
			{ "apply_id", a.id_ },
			{ "applicant_user_id", a.applicant_->uid_ },
			{ "applicant_user_phone", a.applicant_->active_phone },
			{ "state", to_underlying(a.state_) },
			{ "created_at", to_string(a.created_at_) },
			{ "updated_at", to_string(a.updated_at_) },
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


namespace boost::posix_time {
	boost::posix_time::ptime tag_invoke(const boost::json::value_to_tag<boost::posix_time::ptime>&, const boost::json::value& v)
	{
		std::string time_str = boost::json::value_to<std::string>(v);

		boost::smatch w;

		// 2028-03-27T15:34:46+08:00
		boost::regex_match(time_str, w, boost::regex(R"_regex_(([^T]+)T([0-9:]+)\+([0-9][0-9]:[0-9][0-9]))_regex_"));

		std::string date_string, tod_string;
		date_string = w[1];
		tod_string = w[2];
		//call parse_date with first string
		auto  d = boost::date_time::parse_date<boost::posix_time::ptime::date_type>(date_string);
		//call parse_time_duration with remaining string
		auto td = boost::date_time::parse_delimited_time_duration<boost::posix_time::ptime::time_duration_type>(tod_string);
		//construct a time
		return boost::posix_time::ptime(d, td);
	}
}
