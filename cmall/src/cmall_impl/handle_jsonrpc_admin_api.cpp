#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "httpd/http_misc_helper.hpp"
#include "services/search_service.hpp"
#include "services/merchant_git_repo.hpp"
#include "cmall/conversion.hpp"

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_admin_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
    client_connection& this_client = *connection_ptr;
    boost::json::object reply_message;
    services::client_session& session_info = *this_client.session_info;
    cmall_user& this_user				   = *(session_info.user_info);
    boost::ignore_unused(this_user);

    switch (method)
    {
        case req_method::admin_user_list:
		{
            std::vector<cmall_user> all_users;
            using query_t = odb::query<cmall_user>;
            auto query = query_t::deleted_at.is_null() + " order by " + query_t::created_at + " desc";
            co_await m_database.async_load<cmall_user>(query, all_users);
            reply_message["result"] = boost::json::value_from(all_users);
		}
		break;
        case req_method::admin_user_ban:
		{
			auto uid = jsutil::json_accessor(params).get("uid", -1).as_int64();
			if (uid < 0)
                throw boost::system::system_error(cmall::error::invalid_params);

			co_await m_database.async_update<cmall_user>(uid, [](cmall_user&& user) mutable
			{
				user.state_ = user_state_t::banned;
				return user;
			});
			co_await m_database.async_update<cmall_merchant>(uid, [](cmall_merchant&& merchant) mutable
			{
				merchant.state_ = merchant_state_t::banned;
				return merchant;
			});
			reply_message["result"] = true;
		}
		break;
        case req_method::admin_user_detail:
        {
			auto uid = jsutil::json_accessor(params).get("uid", -1).as_int64();
			if (uid < 0)
                throw boost::system::system_error(cmall::error::invalid_params);

			cmall_user u;
            bool found = co_await m_database.async_load<cmall_user>(uid, u);
			if (!found)
				throw boost::system::system_error(cmall::error::user_not_found);

			reply_message["result"] = boost::json::value_from(u);

        }break;
        case req_method::admin_list_merchants:
        {
            std::vector<cmall_merchant> all_merchants;
            using query_t = odb::query<cmall_merchant>;
            auto query = query_t::deleted_at.is_null() + " order by " + query_t::created_at + " desc";
            co_await m_database.async_load<cmall_merchant>(query, all_merchants);
            reply_message["result"] = boost::json::value_from(all_merchants);
        }
        break;
        case req_method::admin_list_applicants:
        {
            std::vector<cmall_apply_for_mechant> all_applicats;
            using query_t = odb::query<cmall_apply_for_mechant>;
            auto query = " 1 = 1 order by " + query_t::created_at + " desc";
            co_await m_database.async_load<cmall_apply_for_mechant>(query, all_applicats);
            reply_message["result"] = boost::json::value_from(all_applicats);
        }break;
        case req_method::admin_approve_merchant:
        {
            auto apply_id = jsutil::json_accessor(params).get("apply_id", -1).as_int64();
            if (apply_id < 0)
                throw boost::system::system_error(cmall::error::invalid_params);

            // 更新申请状态.
            cmall_apply_for_mechant apply;
            cmall_merchant m;

            bool succeed = co_await m_database.async_transacton([&](const cmall_database::odb_transaction_ptr& tx) mutable -> awaitable<bool>
            {
                auto& db = tx->database();

                bool found = db.find(apply_id, apply);
                if (!found)
                    co_return false;
                if (apply.state_ != approve_state_t::waiting)
                    co_return false;

                apply.state_ = approve_state_t::approved;
                apply.deleted_at_ = boost::posix_time::second_clock::local_time();
                db.update(apply);

                // 创建商户.
                auto gitea_password = gen_password();
                m.uid_ = apply.applicant_->uid_;
                m.name_ = apply.applicant_->name_;
                m.state_ = merchant_state_t::normal;
                m.gitea_password = gitea_password;
                m.repo_path = std::filesystem::path(m_config.repo_root / std::format("m{}", m.uid_)  / "shop.git").string();
                db.persist(m);

                co_return true;
            });

            if (succeed)
            {
				// 初始化仓库.
				co_await gitea_service.init_user(m.uid_, m.gitea_password.get(), m_config.gitea_template_user, m_config.gitea_template_reponame);

                co_await boost::asio::co_spawn(background_task_thread_pool, [this, m]() mutable -> awaitable<void>
				{
                    co_await load_merchant_git(m);
                }, use_awaitable);

                co_await send_notify_message(apply_id, R"---({{"topic":"status_updated"}})---",
                    this_client.connection_id_);

            }
            reply_message["result"] = true;
        }
        break;
        case req_method::admin_deny_applicant:
        {
            auto apply_id = jsutil::json_accessor(params).get("apply_id", -1).as_int64();
            auto reason = jsutil::json_accessor(params).get_string("reason");
            if (apply_id < 0)
                throw boost::system::system_error(cmall::error::invalid_params);

            using query_t = odb::query<cmall_apply_for_mechant>;
            auto query = query_t::id == apply_id && query_t::state == approve_state_t::waiting;
            co_await m_database.async_update<cmall_apply_for_mechant>(query, [reason](cmall_apply_for_mechant&& apply) mutable
            {
                apply.state_ = approve_state_t::denied;
                apply.ext_ = reason;
                return apply;
            });
            reply_message["result"] = true;
        }
        break;
        case req_method::admin_disable_merchants:
        case req_method::admin_reenable_merchants:
        {
            bool enable = method == req_method::admin_reenable_merchants;
            auto merchants = jsutil::json_accessor(params).get("merchants", {});
            if (!merchants.is_array())
                throw boost::system::system_error(cmall::error::invalid_params);

            auto marr = merchants.as_array();
            std::vector<std::uint64_t> mids;
            for (const auto& v : marr)
            {
                if (v.is_int64())
                {
                    auto mid = v.as_int64();
                    if (mid >= 0)
                    {
                        mids.push_back(mid);
                    }
                }
            }
            if (!mids.empty())
            {
                auto state = enable ? merchant_state_t::normal : merchant_state_t::disabled;
                using query_t = odb::query<cmall_merchant>;
                auto query = query_t::uid.in_range(mids.begin(), mids.end());
                co_await m_database.async_update<cmall_merchant>(query, [state](cmall_merchant&& m) mutable
                {
                    m.state_ = state;
                    return m;
                });
            }
            reply_message["result"] = true;
        }
        break;
        case req_method::admin_sudo:
        {
            std::uint64_t target_uid;

            auto target_uid_ = jsutil::json_accessor(params).get("target_uid", {});
            if (target_uid_.is_int64())
            {
                target_uid = target_uid_.as_int64();
            }
            else if (target_uid_.is_uint64())
            {
                target_uid = target_uid_.as_uint64();
            }
            else
            {
                throw boost::system::system_error(error::invalid_params);
            }
            if (target_uid == this_user.uid_)
                throw boost::system::system_error(error::cannot_sudo);

            cmall_merchant target_merchant;
            cmall_user target_user;

            if (co_await m_database.async_load<cmall_user>(odb::query<cmall_user>::uid == target_uid, target_user))
            {
                if (co_await m_database.async_load<cmall_merchant>(odb::query<cmall_merchant>::uid == target_uid, target_merchant))
                {
                    // sudo 到 merchant
                    session_info.original_user = session_info.admin_info;
                    session_info.admin_info.reset();
                    session_info.isAdmin = false;
                    session_info.isMerchant = true;
                    session_info.sudo_mode = true;
                    session_info.merchant_info = target_merchant;
                    session_info.user_info = target_user;

                    co_await session_cache_map.save(this_client.session_info->session_id, *this_client.session_info);
                    reply_message["result"] = {
                        { "sudo", "success" },
                        { "isMerchant", session_info.isMerchant },
                        { "isAdmin", session_info.isAdmin },
                    };
                    break;
                }
                else
                {
                    // sudo 到普通用户.
                    session_info.original_user = session_info.admin_info;
                    session_info.admin_info.reset();
                    session_info.isAdmin = false;
                    session_info.isMerchant = false;
                    session_info.sudo_mode = true;
                    session_info.merchant_info.reset();
                    session_info.user_info = target_user;

                    co_await session_cache_map.save(this_client.session_info->session_id, *this_client.session_info);
                    reply_message["result"] = {
                        { "sudo", "success" },
                        { "isMerchant", session_info.isMerchant },
                        { "isAdmin", session_info.isAdmin },
                    };
                    break;
                }
            }
            else
            {
                throw boost::system::system_error(error::internal_server_error);
            }
        }
        break;
        case req_method::admin_sudo_cancel:
        {
            cmall_merchant admin_check_merchant;

            bool admin_is_merchant = co_await m_database.async_load<cmall_merchant>(odb::query<cmall_merchant>::uid == session_info.original_user->user->uid_, admin_check_merchant);

            session_info.isAdmin = true;
            session_info.admin_info = session_info.original_user;
            session_info.user_info = *(session_info.original_user->user);
            session_info.sudo_mode = false;
            session_info.isMerchant = admin_is_merchant;
            if (admin_is_merchant)
                session_info.merchant_info = admin_check_merchant;
            else
                session_info.merchant_info.reset();
            session_info.original_user.reset();
            co_await session_cache_map.save(this_client.session_info->session_id, *this_client.session_info);
            reply_message["result"] = {
                { "sudo", "success" },
                { "isMerchant", session_info.isMerchant },
                { "isAdmin", session_info.isAdmin },
            };
            break;
        }
        case req_method::admin_list_index_goods:
        {

        }break;
        case req_method::admin_add_index_goods:
        {

        }break;
        case req_method::admin_remove_index_goods:
        {

        }break;
        default:
            throw "this should never be executed";
    }

    co_return reply_message;
}
