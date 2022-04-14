
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "httpd/http_misc_helper.hpp"
#include "services/search_service.hpp"
#include "services/merchant_git_repo.hpp"
#include "cmall/conversion.hpp"

awaitable<bool> cmall::cmall_service::order_check_payment(cmall_order& order, const cmall_merchant& seller)
{
    boost::system::error_code ec;
    auto merchant_repo_ptr = get_merchant_git_repo(seller);
    std::string checkpay_script_content = co_await merchant_repo_ptr->get_file_content("scripts/checkpay.js", ec);
    if (!checkpay_script_content.empty())
    {
        std::string pay_check_result = co_await boost::asio::co_spawn(background_task_thread_pool,
            script_runner.run_script(checkpay_script_content, {"--order-id", order.oid_}), use_awaitable);
        if (pay_check_result == "payed" || pay_check_result == "payed\n")
        {
            co_return co_await order_mark_payed(order, seller);
        }
    }
    else
    {
        throw boost::system::system_error(cmall::error::no_paycheck_script_spplyed);
    }
    co_return false;
}

awaitable<bool> cmall::cmall_service::order_mark_payed(cmall_order& order, const cmall_merchant& seller)
{
    order.payed_at_ = boost::posix_time::second_clock::local_time();

    bool success = co_await m_database.async_update<cmall_order>(order);

    if (success)
    {
        boost::system::error_code ec;
        auto merchant_repo_ptr = get_merchant_git_repo(seller, ec);
        if (merchant_repo_ptr)
        {
            std::string pay_script_content = co_await merchant_repo_ptr->get_file_content("scripts/orderstatus.js", ec);
            if (!ec && !pay_script_content.empty())
            {
                boost::asio::co_spawn(background_task_thread_pool,
                    script_runner.run_script(pay_script_content, {"--order-id", order.oid_}), boost::asio::detached);
            }
        }
        co_return true;
    }
    else
    {
        throw boost::system::system_error(cmall::error::internal_server_error);
    }
}

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_merchant_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
    boost::json::object reply_message;
    services::client_session& session_info = *(connection_ptr->session_info);
    cmall_user& this_user				   = *(session_info.user_info);
    cmall_merchant& this_merchant		   = *(session_info.merchant_info);

    switch (method)
    {
        case req_method::merchant_info:
        {
            cmall_merchant m;
            bool found = co_await m_database.async_load(this_merchant.uid_, m);
            if (!found)
                throw boost::system::system_error(cmall::error::merchant_vanished);

            session_info.merchant_info = m;

            reply_message["result"] = boost::json::value_from(m);
        }
        break;
        case req_method::merchant_get_sold_order_detail:
        {
            auto orderid	 = jsutil::json_accessor(params).get_string("orderid");

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::oid == orderid && query_t::seller == this_user.uid_ && query_t::deleted_at.is_null());
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
            if (orders.size() == 1)
                reply_message["result"] = boost::json::value_from(orders[0]);
            else
                throw boost::system::system_error(cmall::error::order_not_found);
        }
        break;
        case req_method::merchant_sold_orders_check_payment:
        {
            auto orderid = jsutil::json_accessor(params).get_string("orderid");

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::oid == orderid && query_t::seller == this_user.uid_ && query_t::deleted_at.is_null());
            co_await m_database.async_load<cmall_order>(query, orders);

            if (orders.size() != 1)
                throw boost::system::system_error(cmall::error::order_not_found);

            if (orders[0].payed_at_.null())
            {
                reply_message["result"] = co_await order_check_payment(orders[0], this_merchant);
            }
            else
            {
                reply_message["result"] = true;
            }
        }
        break;
        case req_method::merchant_sold_orders_mark_payed:
        {
            auto orderid = jsutil::json_accessor(params).get_string("orderid");

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::oid == orderid && query_t::seller == this_user.uid_ && query_t::deleted_at.is_null());
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
            if (orders.size() != 1)
                throw boost::system::system_error(cmall::error::order_not_found);

            if (orders[0].payed_at_.null())
            {
                reply_message["result"] = co_await order_mark_payed(orders[0], this_merchant);
            }
            else
            {
                reply_message["result"] = true;
            }
        }
        break;
        case req_method::merchant_goods_list:
        {
            std::vector<services::product> all_products = co_await get_merchant_git_repo(this_merchant)->get_products(this_merchant.name_);
            reply_message["result"] = boost::json::value_from(all_products);

        }break;
        case req_method::merchant_list_sold_orders:
        {
            auto page	   = jsutil::json_accessor(params).get("page", 0).as_int64();
            auto page_size = jsutil::json_accessor(params).get("page_size", 20).as_int64();
			auto status = jsutil::json_accessor(params).get_string("status"); // unpay / payed / closed

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query = (query_t::seller == this_user.uid_ && query_t::deleted_at.is_null());

			if (status == "unpay")
				query = query && query_t::payed_at.is_null();
			if (status == "payed")
				query = query && query_t::payed_at.is_not_null();
			if (status == "closed")
				query = query && query_t::close_at.is_not_null();

			cmall_order_stat ostat;
			co_await m_database.async_load<cmall_order_stat>(query, ostat);

			query += " order by " + query_t::created_at + " desc " +
                " limit " + std::to_string(page_size) + " offset " + std::to_string(page * page_size);
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
			boost::json::value result {
				{ "orders", boost::json::value_from(orders) },
				{ "count", ostat.count },
			};
            // reply_message["result"] = boost::json::value_from(orders);
            reply_message["result"] = result;
        }
        break;
        case req_method::merchant_sold_orders_add_kuaidi:
        {
            auto orderid	   = jsutil::json_accessor(params).get_string("orderid");
            auto kuaidihao	   = jsutil::json_accessor(params).get_string("kuaidihao");
            auto kuaidigongsi	   = jsutil::json_accessor(params).get_string("kuaidigongsi");

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::oid == orderid && query_t::seller == this_user.uid_ && query_t::deleted_at.is_null());
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
            if (orders.size() == 1)
            {
                cmall_kuaidi_info kuaidi_info;
                kuaidi_info.kuaidigongsi = kuaidigongsi;
                kuaidi_info.kuaidihao = kuaidihao;

                orders[0].kuaidi.push_back(kuaidi_info);

                bool success = co_await m_database.async_update<cmall_order>(orders[0]);

                reply_message["result"] = success;
            }
            else
                throw boost::system::system_error(cmall::error::order_not_found);
        }break;
        case req_method::merchant_delete_sold_orders:
        {
            auto orderid	   = jsutil::json_accessor(params).get_string("orderid");

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::oid == orderid && query_t::seller == this_user.uid_ && query_t::deleted_at.is_null());
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
            if (orders.size() == 1)
            {
                orders[0].payed_at_ = boost::posix_time::second_clock::local_time();

                bool success = co_await m_database.async_soft_remove<cmall_order>(orders[0]);

                reply_message["result"] = success;
            }
            else
                throw boost::system::system_error(cmall::error::order_not_found);
        }
        break;
        case req_method::merchant_get_gitea_password:
            reply_message["result"] = this_merchant.gitea_password.null() ? boost::json::value(nullptr) : boost::json::value(this_merchant.gitea_password.get());
            break;
        case req_method::merchant_reset_gitea_password:
        {
            auto gitea_password = gen_password();
            bool ok = co_await gitea_service.change_password(this_merchant.uid_, gitea_password);
            if (ok)
            {
                this_merchant.gitea_password = gitea_password;
                co_await m_database.async_update<cmall_merchant>(this_merchant.uid_, [&](cmall_merchant&& m) mutable {
                    m.gitea_password = gitea_password;
                    return m;
                });
            }
            reply_message["result"] = ok;
        }
        break;
        case req_method::merchant_create_apptoken:
        {
            cmall_apptoken token;
            token.uid_ = this_user.uid_;
            token.apptoken = gen_uuid();
            bool db_ok = co_await m_database.async_add(token);
            reply_message["result"] = db_ok ? token.apptoken : "" ;
        }break;
		case req_method::merchant_list_apptoken:
        {
            std::vector<cmall_apptoken> r;
            co_await m_database.async_load<cmall_apptoken>(odb::query<cmall_apptoken>::uid == this_user.uid_, r);
            std::vector<std::string> r_converted;
            std::transform(r.begin(), r.end(), std::back_inserter(r_converted), [](auto t){ return t.apptoken; });
            reply_message["result"] = boost::json::value_from(r_converted);
        }break;
		case req_method::merchant_delete_apptoken:
        {
            auto apptoken	   = jsutil::json_accessor(params).get_string("apptoken");
            bool db_ok = co_await m_database.async_erase<cmall_apptoken>(odb::query<cmall_apptoken>::apptoken == apptoken);
            reply_message["result"] = db_ok;
        }break;
        case req_method::merchant_alter_name:
        {
            auto new_name	   = jsutil::json_accessor(params).get_string("name");
            bool db_ok = co_await m_database.async_update<cmall_merchant>(this_merchant.uid_, [new_name](cmall_merchant&& old_value)
            {
                old_value.name_ = new_name;
                return old_value;
            });
            if (db_ok)
                this_merchant.name_ = new_name;
            reply_message["result"] = db_ok;
        }break;
        default:
            throw "this should never be executed";
    }

    co_return reply_message;
}
