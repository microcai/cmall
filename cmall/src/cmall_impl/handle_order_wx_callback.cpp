

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

awaitable<bool> cmall::cmall_service::handle_order_wx_callback(services::weixin::notify_message msg)
{
    cmall_order payed_order;
    bool db_ok = co_await m_database.async_load<cmall_order>(odb::query<cmall_order>::oid == msg.out_trade_no, payed_order);
    if (db_ok)
    {
        if (msg.trade_state == services::weixin::pay_status::SUCCESS)
        {
            payed_order.payed_at_ = msg.success_time;
            co_return co_await m_database.async_update<cmall_order>(payed_order);
        }
    }

    co_return false;
}
