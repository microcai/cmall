
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "services/merchant_git_repo.hpp"
#include "cmall/conversion.hpp"

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_cart_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
    client_connection& this_client = *connection_ptr;
    boost::json::object reply_message;
    services::client_session& session_info = *this_client.session_info;
    cmall_user& this_user				   = *(session_info.user_info);

    switch (method)
    {
        case req_method::cart_add: // 添加到购物车.
        {
            auto merchant_id = jsutil::json_accessor(params).get("merchant_id", -1).as_int64();
            auto goods_id	 = jsutil::json_accessor(params).get_string("goods_id");
            if (merchant_id < 0 || goods_id.empty())
                throw boost::system::system_error(cmall::error::invalid_params);

            cmall_cart item;
            using query_t = odb::query<cmall_cart>;
            auto query(query_t::uid == this_user.uid_ && query_t::merchant_id == merchant_id
                && query_t::goods_id == goods_id);
            if (co_await m_database.async_load<cmall_cart>(query, item))
            {
                item.count_ += 1;
                co_await m_database.async_update(item);
            }
            else
            {
                item.uid_		  = this_user.uid_;
                item.merchant_id_ = merchant_id;
                item.goods_id_	  = goods_id;
                item.count_		  = 1;

                co_await m_database.async_add(item);
            }
            reply_message["result"] = true;

            co_await send_notify_message(this_user.uid_,
                std::format(R"---({{"topic":"cart_changed", "session_id": "{}"}})---",
                    this_client.session_info->session_id),
                this_client.connection_id_);
        }
        break;
        case req_method::cart_mod: // 修改数量.
        {
            auto item_id = jsutil::json_accessor(params).get("item_id", -1).as_int64();
            auto count	 = jsutil::json_accessor(params).get("count", 0).as_int64();
            if (item_id < 0 || count <= 0)
                throw boost::system::system_error(cmall::error::invalid_params);

            cmall_cart item;
            bool ok = co_await m_database.async_load(item_id, item);
            if (!ok)
            {
                throw boost::system::system_error(cmall::error::cart_goods_not_found);
            }
            if (item.uid_ != this_user.uid_)
            {
                throw boost::system::system_error(cmall::error::invalid_params);
            }

            co_await m_database.async_update<cmall_cart>(item_id,
                [count](cmall_cart&& old) mutable
                {
                    old.count_ = count;
                    return old;
                });
            reply_message["result"] = true;
            co_await send_notify_message(this_user.uid_,
                std::format(R"---({{"topic":"cart_changed", "session_id": "{}"}})---",
                    this_client.session_info->session_id),
                this_client.connection_id_);
        }
        break;
        case req_method::cart_del: // 从购物车删除.
        {
            auto item_id = jsutil::json_accessor(params).get("item_id", -1).as_int64();
            if (item_id < 0)
                throw boost::system::system_error(cmall::error::invalid_params);

            cmall_cart item;
            bool ok = co_await m_database.async_load(item_id, item);
            if (!ok)
            {
                throw boost::system::system_error(cmall::error::cart_goods_not_found);
            }
            if (item.uid_ != this_user.uid_)
            {
                throw boost::system::system_error(cmall::error::invalid_params);
            }

            co_await m_database.async_hard_remove<cmall_cart>(item_id);
            reply_message["result"] = true;
            co_await send_notify_message(this_user.uid_,
                std::format(R"---({{"topic":"cart_changed", "session_id": "{}"}})---",
                    this_client.session_info->session_id),
                this_client.connection_id_);
        }
        break;
        case req_method::cart_list: // 查看购物车列表.
        {
            auto page	   = jsutil::json_accessor(params).get("page", 0).as_int64();
            auto page_size = jsutil::json_accessor(params).get("page_size", 20).as_int64();

            std::map<std::uint64_t, cmall_merchant> cache;

            std::vector<cmall_cart> items;
            using query_t = odb::query<cmall_cart>;
            auto query	  = (query_t::uid == this_user.uid_) + " order by " + query_t::created_at + " desc limit "
                + std::to_string(page_size) + " offset " + std::to_string(page * page_size);
            co_await m_database.async_load<cmall_cart>(query, items);

            std::set<std::uint64_t> vanished_merchant_ids;

            for (auto& cc: items)
            {
                if (!cache.contains(cc.merchant_id_))
                {
                    cmall_merchant m;
                    if (! co_await m_database.async_load<cmall_merchant>(cc.merchant_id_, m))
                    {
                        vanished_merchant_ids.insert(cc.merchant_id_);
                        continue;
                    }
                    cache.emplace(cc.merchant_id_, m);
                }

                cc.merchant_name_ = cache[cc.merchant_id_].name_;
            }

            if (!vanished_merchant_ids.empty())
            {
                for (auto merchant_id : vanished_merchant_ids)
                    co_await m_database.async_erase<cmall_cart>(query_t::merchant_id == merchant_id);
            }

            reply_message["result"] = boost::json::value_from(items);
        }
        break;
        default:
            throw "this should never be executed";
    }
    co_return reply_message;
}
