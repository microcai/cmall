
#pragma once

#include <boost/system/error_code.hpp>

namespace cmall { namespace error{

	enum errc_t
	{
		/// wake_up called
		internal_server_error = 1,
		invalid_json,
		unknown_method,
		invalid_verify_code,
		session_needed,
		login_required,
		merchant_user_required,
		admin_user_required,
		not_implemented,
		recipient_id_out_of_range, 
		duplicate_operation,
		already_exist,
		invalid_params,
		order_not_found, 
		goods_not_found,
		cart_goods_not_found,
	};

	class cmall_category : public boost::system::error_category
	{
	public:
		const char *name() const noexcept { return "cmall"; }
		std::string message(int ev) const;
	};

	const boost::system::error_category& error_category();


	inline boost::system::error_code make_error_code(errc_t e)
	{
		return boost::system::error_code(static_cast<int>(e), error_category());
	}

}}

namespace boost
{
  namespace system
  {
    // Tell the C++ 11 STL metaprogramming that enum ConversionErrc
    // is registered with the standard error code system
    template <> struct is_error_code_enum<cmall::error::errc_t> : std::true_type
    {
    };
  }  // namespace system
}  // namespace boost
