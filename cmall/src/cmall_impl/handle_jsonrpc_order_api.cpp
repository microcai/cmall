

#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "httpd/http_misc_helper.hpp"
#include "services/search_service.hpp"
#include "services/merchant_git_repo.hpp"
#include "cmall/conversion.hpp"
#include "cmall/database.hpp"

inline bool is_sigle_merchant_order(const cmall_order& order_to_pay)
{
    auto first_merchant_id =  order_to_pay.bought_goods[0].merchant_id;

    auto finded = std::find_if(order_to_pay.bought_goods.begin(), order_to_pay.bought_goods.end(), [=](auto & item){ return item.merchant_id != first_merchant_id; });
    return finded==order_to_pay.bought_goods.end();
}

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

            cpp_numeric total_price = 0, total_kuaidifei = 0;

            std::map<std::uint64_t, cmall_merchant> cache;

            for (boost::json::value goods_v : goods_array_ref)
            {
                boost::json::object goods_ref = goods_v.as_object();
                auto merchant_id_of_goods	  = goods_ref["merchant_id"].as_int64();
                auto goods_id_of_goods		  = jsutil::json_as_string(goods_ref["goods_id"].as_string(), "");

                boost::json::array options_of_goods;

                if (goods_ref.contains("options"))
                    options_of_goods = goods_ref["options"].as_array();

                if (!cache.contains(merchant_id_of_goods))
                {
                    cmall_merchant m;
                    if (!co_await m_database.async_load<cmall_merchant>(merchant_id_of_goods, m))
                        continue;
                    cache.emplace(merchant_id_of_goods, m);
                }

                cmall_merchant m = cache[merchant_id_of_goods];
                if (m.state_ != merchant_state_t::normal)
                    continue;

                boost::system::error_code ec;
                services::product product_in_mall
                    = co_await get_merchant_git_repo(merchant_id_of_goods)->get_product(goods_id_of_goods, m.name_, this_client.ws_client->baseurl_);

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

                if (!product_in_mall.options.empty())
                {
                    if (options_of_goods.size() != product_in_mall.options.size())
                        throw boost::system::system_error(cmall::error::good_option_needed);

                    for (auto option : product_in_mall.options)
                    {
                        // 选择和商品提供的选择, 必须相同.
                        auto o_it = std::find_if(options_of_goods.begin(), options_of_goods.end(), [option](const boost::json::value& v){
                            return v.as_object().at("option").as_string() == option.option_title;
                        });
                        if (o_it == options_of_goods.end())
                            throw boost::system::system_error(cmall::error::good_option_invalid);
                        // option.option_title
                        std::string user_select = jsutil::json_as_string(o_it->as_object().at("select"), "");

                        auto s_it = std::find_if(option.selections.begin(), option.selections.end(), [&](auto s){
                            return s.selection_name == user_select;
                        });

                        if (s_it == option.selections.end())
                            throw boost::system::system_error(cmall::error::good_option_invalid);

                        good_snap.price_ += s_it->selection_price;
                        good_snap.selection += fmt::format("{}:{};", option.option_title, s_it->selection_name);
                    }
                }

                new_order.bought_goods.push_back(good_snap);

                total_price += good_snap.price_;
                if (!product_in_mall.kuaidifei.empty())
                    total_kuaidifei += cpp_numeric(product_in_mall.kuaidifei);
            }

            new_order.price_ = total_price;
            new_order.kuaidifei = total_kuaidifei;

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
			auto status = jsutil::json_accessor(params).get_string("status"); // unpay / payed / closed

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::buyer == this_user.uid_ && query_t::deleted_at.is_null());

			if (status == "unpay")
				query = query && query_t::payed_at.is_null();
			if (status == "payed")
				query = query && query_t::payed_at.is_not_null();
			if (status == "closed")
				query = query && query_t::close_at.is_not_null();

			cmall_order_stat ostat;
			co_await m_database.async_load<cmall_order_stat>(query, ostat);

			query +=  " order by " + query_t::created_at + " desc limit " + std::to_string(page_size) + " offset "
                + std::to_string(page * page_size);
            co_await m_database.async_load<cmall_order>(query, orders);

            LOG_DBG << "order_list retrieved, " << orders.size() << " items";
			boost::json::value result {
				{ "orders", boost::json::value_from(orders) },
				{ "count", ostat.count },
			};
            reply_message["result"] = result;
        }
        break;
        case req_method::order_get_paymethods:
        {
            auto orderid = jsutil::json_accessor(params).get_string("orderid");
			cmall_order order_to_pay;
			using query_t	   = odb::query<cmall_order>;
			auto query		   = query_t::oid == orderid && query_t::buyer == this_user.uid_;
			bool order_founded = co_await m_database.async_load<cmall_order>(query, order_to_pay);
			if (!order_founded)
			{
				throw boost::system::system_error(cmall::error::order_not_found);
			}
            if (!is_sigle_merchant_order(order_to_pay))
                throw boost::system::system_error(cmall::error::combile_order_not_supported);

			boost::system::error_code ec;
			std::uint64_t merchant_id = order_to_pay.bought_goods[0].merchant_id;
			auto methods = co_await get_merchant_git_repo(merchant_id)->get_supported_payment();
            if (methods.empty())
                throw boost::system::system_error(error::merchant_has_no_payments_supported);
            reply_message["result"] = boost::json::value_from(methods);
        }break;
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

            if (!is_sigle_merchant_order(order_to_pay))
                throw boost::system::system_error(cmall::error::combile_order_not_supported);

            // 对已经存在的订单, 获取支付连接.
            boost::system::error_code ec;
            std::uint64_t merchant_id = order_to_pay.bought_goods[0].merchant_id;

            std::string pay_script_content
                = co_await get_merchant_git_repo(merchant_id)->get_file_content("scripts/getpayurl.js", ec);
            services::payment_url payurl = co_await payment_service.get_payurl(pay_script_content, orderid, 0, to_string(order_to_pay.price_ + order_to_pay.kuaidifei), paymentmethod);

            reply_message["result"] = { { "type", "url" }, { "url", payurl.uri } };
        }
        break;
        case req_method::order_get_wxpay_prepay_id:
        {
            auto wx_microapp_appid = jsutil::json_accessor(params).get_string("wx_microapp_appid");
            auto orderid = jsutil::json_accessor(params).get_string("orderid");
            auto payer_openid = jsutil::json_accessor(params).get_string("payer_openid");

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

            if (!is_sigle_merchant_order(order_to_pay))
                throw boost::system::system_error(cmall::error::combile_order_not_supported);

            cmall_merchant order_merchant;

            auto ok = co_await m_database.async_load<cmall_merchant>(merchant_id, order_merchant);
            if (!ok)
                throw boost::system::system_error(cmall::error::merchant_vanished);

            if (!order_merchant.exinfo_wx_mchid)
                throw boost::system::system_error(cmall::error::merchant_does_not_support_microapp_wxpay);

			std::shared_ptr<services::wxpay_service> wxpay_for_selected_appid;
			{
				std::shared_lock<std::shared_mutex> l (wxpay_services);
				auto it = wxpay_services.find(wx_microapp_appid);
				if (wxpay_services.end() != it)
					wxpay_for_selected_appid = it->second;
			}

			if (!wxpay_for_selected_appid)
                throw boost::system::system_error(cmall::error::merchant_does_not_support_microapp_wxpay);

            std::string prepay_id = co_await wxpay_for_selected_appid->get_prepay_id(order_merchant.exinfo_wx_mchid.get(), orderid, order_to_pay.price_ + order_to_pay.kuaidifei, order_to_pay.bought_goods[0].description_, payer_openid);
            if (prepay_id.empty())
                throw boost::system::system_error(cmall::error::wxpay_server_error);
            else
                reply_message["result"] = prepay_id;

        }break;
        case req_method::order_get_wxpay_object:
        {
            auto appid = jsutil::json_accessor(params).get_string("wx_microapp_appid");
			std::shared_ptr<services::wxpay_service> wxpay_for_selected_appid;
			{
				std::shared_lock<std::shared_mutex> l (wxpay_services);
				auto it = wxpay_services.find(appid);
				if (wxpay_services.end() != it)
					wxpay_for_selected_appid = it->second;
			}

			if (!wxpay_for_selected_appid)
                throw boost::system::system_error(cmall::error::merchant_does_not_support_microapp_wxpay);
            auto prepay_id = jsutil::json_accessor(params).get_string("prepay_id");
            reply_message["result"] = co_await wxpay_for_selected_appid->get_pay_object(prepay_id);
        }
        break;
        case req_method::order_check_payment:
        {
            auto orderid = jsutil::json_accessor(params).get_string("orderid");

            std::vector<cmall_order> orders;
            using query_t = odb::query<cmall_order>;
            auto query	  = (query_t::oid == orderid && query_t::buyer == this_user.uid_ && query_t::deleted_at.is_null());
            co_await m_database.async_load<cmall_order>(query, orders);

            if (orders.size() != 1)
                throw boost::system::system_error(cmall::error::order_not_found);

            if (orders[0].payed_at_.null())
            {
                cmall_merchant seller_merchant;
                if (co_await m_database.async_load<cmall_merchant>(orders[0].seller_, seller_merchant))
                {
                    reply_message["result"] = co_await order_check_payment(orders[0], seller_merchant);
                }
                else
                    reply_message["result"] = false;
            }
            else
            {
                reply_message["result"] = true;
                co_return reply_message;
            }
        }
        break;
        default:
            throw "this should never be executed";
    }
    co_return reply_message;
}
