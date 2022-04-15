
#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "cmall/conversion.hpp"
#include "httpd/http_misc_helper.hpp"
#include "services/merchant_git_repo.hpp"
#include "services/search_service.hpp"

awaitable<boost::json::object> cmall::cmall_service::handle_jsonrpc_goods_api(
	client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
{
	client_connection& this_client = *connection_ptr;
	boost::json::object reply_message;
	services::client_session& session_info = *this_client.session_info;
	boost::ignore_unused(session_info);

	switch (method)
	{
		case req_method::search_goods:
		{
			auto q			   = jsutil::json_accessor(params).get_string("q");
			auto search_result = co_await search_service.search_goods(q);

			// then transform goods_ref to products
			std::vector<services::product> final_result;
			std::map<std::uint64_t, cmall_merchant> cache;
			for (auto gr : search_result)
			{
				if (!cache.contains(gr.merchant_id))
				{
					cmall_merchant m;
					co_await m_database.async_load<cmall_merchant>(gr.merchant_id, m);
					cache.emplace(gr.merchant_id, m);
				}
				final_result.push_back(
					co_await get_merchant_git_repo(gr.merchant_id)
						->get_product(gr.goods_id, cache[gr.merchant_id].name_, this_client.ws_client->baseurl_));
			}

			reply_message["result"] = boost::json::value_from(final_result);
		}
		break;
		case req_method::goods_list:
		{
			// 列出 商品, 根据参数决定是首页还是商户
			auto merchant	 = jsutil::json_accessor(params).get_string("merchant");
			auto merchant_id = parse_number<std::uint64_t>(merchant);

			std::vector<cmall_merchant> merchants;
			using query_t = odb::query<cmall_merchant>;
			auto query	  = (query_t::state == merchant_state_t::normal) && (query_t::deleted_at.is_null());
			if (merchant_id.has_value())
			{
				query = query_t::uid == merchant_id.value() && query;
			}
			query += " order by uid desc"; // 优先显示新商户的商品.

			co_await m_database.async_load<cmall_merchant>(query, merchants);

			std::map<std::uint64_t, cmall_merchant> cache;

			std::vector<services::product> all_products;
			if (merchants.size() > 0)
			{
				for (const auto& m : merchants)
				{
					boost::system::error_code ec;
					auto repo = get_merchant_git_repo(m.uid_, ec);
					if (ec)
						continue;
					std::vector<services::product> products
						= co_await repo->get_products(m.name_, this_client.ws_client->baseurl_);
					std::copy(products.begin(), products.end(), std::back_inserter(all_products));
				}
			}

			reply_message["result"] = boost::json::value_from(all_products);
		}
		break;
		case req_method::goods_detail:
        try
        {
            auto merchant_id = jsutil::json_accessor(params).get("merchant_id", -1).as_int64();
            auto goods_id	 = httpd::decodeURIComponent(jsutil::json_accessor(params).get_string("goods_id"));

            if (goods_id.empty())
                throw boost::system::system_error(cmall::error::invalid_params);

            cmall_merchant m;
            if (co_await m_database.async_load(merchant_id, m))
            {
                auto product = co_await get_merchant_git_repo(merchant_id)
                                    ->get_product(goods_id, m.name_, this_client.ws_client->baseurl_);

                // 获取商品信息, 注意这个不是商品描述, 而是商品 标题, 价格, 和缩略图. 用在商品列表页面.
                reply_message["result"] = boost::json::value_from(product);
            }
            else
            {
                throw boost::system::system_error(cmall::error::merchant_vanished);
            }
        }
        catch (std::invalid_argument&)
        {
            throw boost::system::system_error(error::invalid_params);
        }
        break;
		case req_method::goods_markdown:
		{
			auto merchant_id = jsutil::json_accessor(params).get("merchant_id", -1).as_int64();
			auto goods_id	 = jsutil::json_accessor(params).get_string("goods_id");
			if (goods_id.empty())
				throw boost::system::system_error(cmall::error::invalid_params);

			auto merchant_repo_ptr = get_merchant_git_repo(merchant_id);
			std::string product_detail
				= co_await merchant_repo_ptr->get_product_detail(goods_id, connection_ptr->ws_client->baseurl_);
			reply_message["result"] = product_detail;
		}
		break;
		default:
			throw "this should never be executed";
	}

	co_return reply_message;
}
