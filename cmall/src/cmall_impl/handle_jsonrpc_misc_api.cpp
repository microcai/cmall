
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "cmall/conversion.hpp"

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_misc_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
	client_connection& this_client = *connection_ptr;
	boost::json::object reply_message;
	services::client_session& session_info = *this_client.session_info;

	auto handle_user_login = [&](std::string telephone) -> awaitable<void>
	{
		// SUCCESS.
		cmall_user user;
		using query_t = odb::query<cmall_user>;
		if (co_await m_database.async_load<cmall_user>(
				query_t::active_phone == telephone, user))
		{
			session_info.user_info = user;

			cmall_merchant merchant_user;
			administrators admin_user;

			// 如果是 merchant/admin 也载入他们的信息
			if (co_await m_database.async_load<cmall_merchant>(user.uid_, merchant_user))
			{
				if (merchant_user.state_ == merchant_state_t::normal)
				{
					session_info.merchant_info = merchant_user;
					session_info.isMerchant	   = true;
				}
			}
			if (co_await m_database.async_load<administrators>(user.uid_, admin_user))
			{
				session_info.admin_info = admin_user;
				session_info.isAdmin	= true;
			}
		}
		else
		{
			cmall_user new_user;
			new_user.active_phone = this_client.session_info->verify_telephone;

			// 新用户注册，赶紧创建个新用户
			co_await m_database.async_add(new_user);
			this_client.session_info->user_info = new_user;

			auto ex = co_await boost::asio::this_coro::executor;
			std::call_once(check_admin_flag, [this, new_user, ex](){
				boost::asio::co_spawn(ex, [this, new_user]() mutable -> awaitable<void>
				{
					// find admin
					std::vector<administrators> admins;
					auto ok = co_await m_database.async_load_all<administrators>(admins);
					if (ok && admins.empty())
					{
						administrators admin;
						admin.uid_ = new_user.uid_;
						admin.user = std::make_shared<cmall_user>(std::move(new_user));
						co_await m_database.async_add(admin);
					}
				}, boost::asio::detached);
			});
		}
		session_info.verify_session_cookie = {};
		// 认证成功后， sessionid 写入 mdbx 数据库以便日后恢复.
		co_await session_cache_map.save(this_client.session_info->session_id, *this_client.session_info);
		reply_message["result"] = {
			{ "login", "success" },
			{ "isMerchant", session_info.isMerchant },
			{ "isAdmin", session_info.isAdmin },
			{ "isSudo", session_info.sudo_mode },
		};

		std::unique_lock<std::shared_mutex> l(active_users);
		active_users.push_back(connection_ptr);
		co_return;
	};

	switch (method)
	{
		case req_method::wx_direct_pay:
		{
            auto appid = jsutil::json_accessor(params).get_string("appid");
			std::shared_ptr<services::wxpay_service> wxpay_for_selected_appid;
			{
				std::shared_lock<std::shared_mutex> l (wxpay_services);
				wxpay_for_selected_appid = wxpay_services[appid];
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
