
#pragma once

#include <boost/system/error_code.hpp>

namespace cmall
{
	namespace error
	{
		enum errc_t
		{
			/// wake_up called
			internal_server_error = 1,
			invalid_json,
			invalid_params,
			unknown_method,
			not_implemented,
			session_needed,
			apitoken_invalid,
			invalid_verify_code,
			login_required,
			merchant_user_required,
			admin_user_required,
			recipient_id_out_of_range,
			duplicate_operation,
			already_exist,
			already_a_merchant,
			order_not_found,
			merchant_vanished,
			goods_not_found,
			cart_goods_not_found,
			already_in_cart,
			should_be_same_merchant,
			no_payment_script_supplyed,
			no_paycheck_script_spplyed,
			merchant_has_no_payments_supported,
			user_not_found,
			user_banned,
			not_in_sudo_mode,
			cannot_sudo,
			kv_store_key_not_found,
			object_too_large,
			merchcant_git_error,
			merchcant_gitrepo_notfound,
			combile_order_not_supported,
			merchant_does_not_support_microapp_wxpay,
			wxpay_server_error,
			nofity_message_signature_invalid,
			nofity_message_decode_failed,
			good_option_needed,
			good_option_invalid,
		};

		class cmall_category : public boost::system::error_category
		{
		public:
			const char* name() const noexcept { return "cmall"; }
			std::string message(int ev) const;
		};

		const boost::system::error_category& error_category();

		inline boost::system::error_code make_error_code(errc_t e)
		{
			return boost::system::error_code(static_cast<int>(e), error_category());
		}

	}
}

namespace boost
{
	namespace system
	{
		// Tell the C++ 11 STL metaprogramming that enum ConversionErrc
		// is registered with the standard error code system
		template <>
		struct is_error_code_enum<cmall::error::errc_t> : std::true_type
		{
		};
	} // namespace system
} // namespace boost
