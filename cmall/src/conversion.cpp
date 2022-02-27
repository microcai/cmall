
#include "cmall/conversion.hpp"
#include "cmall/misc.hpp"


namespace conversion
{

	void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const cmall_user& u)
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
	void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Recipient& r)
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

}