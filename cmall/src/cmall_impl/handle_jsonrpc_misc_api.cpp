
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "cmall/conversion.hpp"

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_misc_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
	client_connection& this_client = *connection_ptr;
	boost::json::object reply_message;
	services::client_session& session_info = *this_client.session_info;

	switch (method)
	{
		case req_method::wx_direct_pay:
		{
            auto appid = jsutil::json_accessor(params).get_string("appid");
			std::shared_ptr<services::wxpay_service> wxpay_for_selected_appid;
			{
				std::shared_lock<std::shared_mutex> l (wxpay_services);
				auto it = wxpay_services.find(appid);
				if (wxpay_services.end() != it)
					wxpay_for_selected_appid = it->second;
			}

			if (!wxpay_for_selected_appid)
                throw boost::system::system_error(cmall::error::merchant_does_not_support_microapp_wxpay);

            auto amount_str = jsutil::json_accessor(params).get_string("amount");
            auto payer_openid = jsutil::json_accessor(params).get_string("payer_openid");
            auto sub_mchid = jsutil::json_accessor(params).get_string("sub_mchid");
			auto orderid = gen_uuid();
            std::string prepay_id = co_await wxpay_for_selected_appid->get_prepay_id(sub_mchid, orderid, cpp_numeric(amount_str), std::string("向商户直接付款"), payer_openid);
            if (prepay_id.empty())
                throw boost::system::system_error(cmall::error::wxpay_server_error);
            else
                reply_message["result"] = co_await wxpay_for_selected_appid->get_pay_object(prepay_id);
		}
		break;
		default:
			throw "this should never be executed";
	}

	co_return reply_message;
}
