

#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "httpd/http_misc_helper.hpp"
#include "services/search_service.hpp"
#include "services/repo_products.hpp"
#include "cmall/conversion.hpp"
#include "cmall/database.hpp"

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_order_api(
    client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
    client_connection& this_client = *connection_ptr;
    boost::json::object reply_message;
    services::client_session& session_info = *this_client.session_info;
    cmall_user& this_user				   = *(session_info.user_info);

    switch (method)
    {
        case req_method::order_create_cart:
        case req_method::order_create_direct:
        {
            // 重新载入 user_info, 以便获取正确的收件人地址信息.
            co_await m_database.async_load<cmall_user>(this_user.uid_, *this_client.session_info->user_info);
            cmall_user& user_info = *(this_client.session_info->user_info);

            boost::json::array goods_array_ref
                = jsutil::json_accessor(params).get("goods", boost::json::array{}).as_array();

            auto recipient_id = jsutil::json_accessor(params).get("recipient_id", -1).as_int64();

            if (!(recipient_id >= 0 && recipient_id < (int64_t)user_info.recipients.size()))
            {
                throw boost::system::system_error(cmall::error::recipient_id_out_of_range);
            }

            cmall_order new_order;
            new_order.recipient.push_back(user_info.recipients[recipient_id]);
            new_order.buyer_ = user_info.uid_;
            new_order.oid_	 = gen_uuid();
            new_order.stage_ = order_unpay;
            new_order.seller_ = 0;

            cpp_numeric total_price = 0;

            for (boost::json::value goods_v : goods_array_ref)
            {
                boost::json::object goods_ref = goods_v.as_object();
                auto merchant_id_of_goods	  = goods_ref["merchant_id"].as_int64();
                auto goods_id_of_goods		  = jsutil::json_as_string(goods_ref["goods_id"].as_string(), "");

                cmall_merchant m;
                bool found = co_await m_database.async_load(merchant_id_of_goods, m);
                if (!found || (m.state_ != merchant_state_t::normal))
                    continue;

                boost::system::error_code ec;
                services::product product_in_mall
                    = co_await get_merchant_git_repo(merchant_id_of_goods)->get_product(goods_id_of_goods);

                if (new_order.seller_ == 0)
                    new_order.seller_ = merchant_id_of_goods;
                else if ( static_cast<std::int64_t>(new_order.seller_) != merchant_id_of_goods)
                {
                    // TODO, 如果不是同一个商户的东西, 是否可以直接变成多个订单?
                    throw boost::system::system_error(cmall::error::should_be_same_merchant);
                }

                new_order.snap_git_version = product_in_mall.git_version;

                goods_snapshot good_snap;

                good_snap.description_	   = product_in_mall.product_description;
                good_snap.good_version_git = product_in_mall.git_version;
                good_snap.name_			   = product_in_mall.product_title;
                good_snap.merchant_id	   = merchant_id_of_goods;
                good_snap.goods_id		   = product_in_mall.product_id;
                good_snap.price_		   = cpp_numeric(product_in_mall.product_price);
                new_order.bought_goods.push_back(good_snap);

                total_price += good_snap.price_;
            }

            new_order.price_ = total_price;

            co_await m_database.async_add(new_order);

            reply_message["result"] = {
                { "orderid", new_order.oid_ },
            };
        }
        break;
        case req_method::order_detail:
        {
            auto orderid	   = jsutil::json_accessor(params).get_string("orderid");

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::oid == orderid && query_t::buyer == this_user.uid_ && query_t::deleted_at.is_null());
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
            if (orders.size() == 1)
                reply_message["result"] = boost::json::value_from(orders[0]);
            else
                throw boost::system::system_error(cmall::error::order_not_found);
        }
        break;
        case req_method::order_close:
            break;
        case req_method::order_list:
        {
            auto page	   = jsutil::json_accessor(params).get("page", 0).as_int64();
            auto page_size = jsutil::json_accessor(params).get("page_size", 20).as_int64();

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::buyer == this_user.uid_ && query_t::deleted_at.is_null()) + " order by "
                + query_t::created_at + " desc limit " + std::to_string(page_size) + " offset "
                + std::to_string(page * page_size);
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
            reply_message["result"] = boost::json::value_from(orders);
        }
        break;
        case req_method::order_get_pay_url:
        {
            auto orderid = jsutil::json_accessor(params).get_string("orderid");
            auto payment = jsutil::json_accessor(params).get_string("payment");
            auto paymentmethod = jsutil::json_accessor(params).get_string("payment");

            cmall_order order_to_pay;
            using query_t	   = odb::query<cmall_order>;
            auto query		   = query_t::oid == orderid && query_t::buyer == this_user.uid_;
            bool order_founded = co_await m_database.async_load<cmall_order>(query, order_to_pay);

            if (!order_founded)
            {
                throw boost::system::system_error(cmall::error::order_not_found);
            }

            // 对已经存在的订单, 获取支付连接.
            boost::system::error_code ec;
            std::uint64_t merchant_id = order_to_pay.bought_goods[0].merchant_id;

            std::string pay_script_content
                = co_await get_merchant_git_repo(merchant_id)->get_file_content("scripts/getpayurl.js", ec);
            services::payment_url payurl = co_await payment_service.get_payurl(pay_script_content, orderid, 0, to_string(order_to_pay.price_), paymentmethod);

            reply_message["result"] = { { "type", "url" }, { "url", payurl.uri } };
        }
        break;
        default:
            throw "this should never be executed";
    }
    co_return reply_message;
}
