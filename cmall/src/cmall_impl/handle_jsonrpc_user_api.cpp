
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "cmall/conversion.hpp"
#include "httpd/http_misc_helper.hpp"
#include "services/repo_products.hpp"
#include "services/search_service.hpp"

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_user_api(
	client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
	client_connection& this_client = *connection_ptr;
	boost::json::object reply_message;
	services::client_session& session_info = *this_client.session_info;

	switch (method)
	{
		case req_method::user_prelogin:
		{
			services::verify_session verify_session_cookie;
			// 拿到 verify_code_token,  要和 verify_code 一并传给 user.login
			// verify_code_token 的有效期为 2 分钟.
			// verify_code_token 对同一个手机号只能一分钟内请求一次.

			std::string tel = jsutil::json_as_string(jsutil::json_accessor(params).get("telephone", ""));

			session_info.verify_telephone = tel;
			verify_session_cookie		  = co_await telephone_verifier.send_verify_code(tel);

			session_info.verify_session_cookie = verify_session_cookie;
			reply_message["result"]			   = true;
		}
		break;
		case req_method::user_login:
		{
			std::string verify_code = jsutil::json_accessor(params).get_string("verify_code");

			if (session_info.verify_session_cookie)
			{
				if (co_await telephone_verifier.verify_verify_code(
						verify_code, this_client.session_info->verify_session_cookie.value()))
				{
					// SUCCESS.
					cmall_user user;
					using query_t = odb::query<cmall_user>;
					if (co_await m_database.async_load<cmall_user>(
							query_t::active_phone == session_info.verify_telephone, user))
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
					}
					session_info.verify_session_cookie = {};
					// 认证成功后， sessionid 写入 mdbx 数据库以便日后恢复.
					co_await session_cache_map.save(this_client.session_info->session_id, *this_client.session_info);
					reply_message["result"] = {
						{ "login", "success" },
						{ "isMerchant", session_info.isMerchant },
						{ "isAdmin", session_info.isAdmin },
					};

					std::unique_lock<std::shared_mutex> l(active_users_mtx);
					active_users.push_back(connection_ptr);
					break;
				}
			}
			throw boost::system::system_error(cmall::error::invalid_verify_code);
		}
		break;

		case req_method::user_logout:
		{
			{
				std::unique_lock<std::shared_mutex> l(active_users_mtx);
				active_users.get<1>().erase(connection_ptr->connection_id_);
			}
			this_client.session_info->clear();
			co_await session_cache_map.save(this_client.session_info->session_id, *this_client.session_info);
			reply_message["result"] = { "status", "success" };
		}
		break;
		case req_method::user_islogin:
		{
			reply_message["result"] = static_cast<bool>(session_info.user_info);
		}
		break;
		case req_method::user_info:
		{
			auto uid = this_client.session_info->user_info->uid_;
			cmall_user uinfo;
			co_await m_database.async_load<cmall_user>(uid, uinfo);
			reply_message["result"] = boost::json::value_from(uinfo);
		}
		break;
		case req_method::user_apply_merchant:
		{
			auto uid = session_info.user_info->uid_;
			cmall_merchant m;
			if (co_await m_database.async_load<cmall_merchant>(uid, m))
			{
				// already a merchant
				throw boost::system::system_error(cmall::error::already_a_merchant);
			}

			using query_t = odb::query<cmall_apply_for_mechant>;

			cmall_apply_for_mechant apply;
			max_application_seq max_seq;
			if (co_await m_database.async_load<max_application_seq>(
					query_t::applicant == session_info.user_info->uid_, max_seq))
			{
                if (max_seq.last_seq.null())
                    apply.seq = 0;
                else
    				apply.seq = max_seq.last_seq.get() + 1;
			}
			else
			{
				apply.seq = 0;
			}

			auto query = (query_t::applicant->uid == session_info.user_info->uid_
				&& query_t::state == approve_state_t::waiting);
			if (co_await m_database.async_load<cmall_apply_for_mechant>(query, apply))
			{
				throw boost::system::system_error(cmall::error::already_exist);
			}

			apply.applicant_ = std::make_shared<cmall_user>(this_client.session_info->user_info.value());
            // 这时候, 如果 seq 序号已经存在, 说明这里被并发了.
            // 于是数据库操作自然就会抛异常. true 也就永远不会返回给客户端了.
			co_await m_database.async_add(apply);
			reply_message["result"] = true;
		}
		break;
		case req_method::user_apply_info:
		{
			auto uid = session_info.user_info->uid_;

			std::vector<cmall_apply_for_mechant> applys;
			using query_t = odb::query<cmall_apply_for_mechant>;
			auto query	  = (query_t::applicant->uid == uid && query_t::deleted_at.is_null()) + " order by "
				+ query_t::created_at + " desc";
			co_await m_database.async_load<cmall_apply_for_mechant>(query, applys);

			reply_message["result"] = boost::json::value_from(applys);
		}
		break;
		case req_method::user_list_recipient_address:
		{
			// 重新载入 user_info, 以便获取正确的收件人地址信息.
			co_await m_database.async_load<cmall_user>(session_info.user_info->uid_, *(session_info.user_info));
			cmall_user& user_info = *(session_info.user_info);
			boost::json::array recipients_array;
			for (std::size_t i = 0; i < user_info.recipients.size(); i++)
			{
				auto jsobj				= boost::json::value_from(user_info.recipients[i]);
				jsobj.as_object()["id"] = i;
				recipients_array.push_back(jsobj);
			}
			reply_message["result"] = recipients_array;
		}
		break;
		case req_method::user_add_recipient_address:
		{
			Recipient new_address;

			// 这次这里获取不到就 throw 出去， 给客户端一点颜色 see see.
			new_address.telephone = params["telephone"].as_string();
			new_address.address	  = params["address"].as_string();
			new_address.name	  = params["name"].as_string();

			cmall_user& user_info = *(session_info.user_info);
			// 重新载入 user_info, 以便获取正确的收件人地址信息.
			bool is_db_op_ok		= co_await m_database.async_update<cmall_user>(user_info.uid_,
				   [&](cmall_user&& value)
				   {
					   value.recipients.push_back(new_address);
					   user_info = value;
					   return value;
				   });
			reply_message["result"] = is_db_op_ok;
		}
		break;
		case req_method::user_modify_receipt_address:
			break;
		case req_method::user_erase_receipt_address:
		{
			auto recipient_id_to_remove = params["recipient_id"].as_int64();
			cmall_user& user_info		= *(session_info.user_info);

			bool is_db_op_ok		= co_await m_database.async_update<cmall_user>(user_info.uid_,
				   [&](cmall_user&& value)
				   {
					   if (recipient_id_to_remove >= 0 && recipient_id_to_remove < (int64_t)value.recipients.size())
						   value.recipients.erase(value.recipients.begin() + recipient_id_to_remove);
					   user_info = value;
					   return value;
				   });
			reply_message["result"] = is_db_op_ok;
		}
		break;
		default:
			throw "this should never be executed";
	}

	co_return reply_message;
}
