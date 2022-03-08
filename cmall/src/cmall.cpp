
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/awaitable.hpp>

#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <boost/scope_exit.hpp>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "boost/asio/io_context.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/redirect_error.hpp"
#include "boost/asio/system_context.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/json/value_from.hpp"
#include "boost/system/detail/error_code.hpp"
#include "boost/system/system_error.hpp"
#include "boost/throw_exception.hpp"
#include "cmall/error_code.hpp"
#include "httpd/acceptor.hpp"
#include "utils/async_connect.hpp"
#include "utils/scoped_exit.hpp"
#include "utils/url_parser.hpp"
#include "utils/http_misc_helper.hpp"

#include "cmall/cmall.hpp"
#include "cmall/database.hpp"
#include "cmall/db.hpp"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#endif

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/printf.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "cmall/http_static_file_handle.hpp"
#include "cmall/js_util.hpp"
#include "magic_enum.hpp"

#include "services/repo_products.hpp"

#include "cmall/conversion.hpp"

#include "httpd/httpd.hpp"

namespace cmall
{
	using namespace std::chrono_literals;

	//////////////////////////////////////////////////////////////////////////

	cmall_service::cmall_service(io_context_pool& ios, const server_config& config)
		: m_io_context_pool(ios)
		, m_io_context(m_io_context_pool.server_io_context())
		, m_config(config)
		, m_database(m_config.dbcfg_)
		, git_operation_thread_pool(8)
		, session_cache_map(m_config.session_cache_file)
		, telephone_verifier(m_io_context)
		, payment_service(m_io_context)
	{
	}

	cmall_service::~cmall_service() { LOG_DBG << "~cmall_service()"; }

	boost::asio::awaitable<void> cmall_service::stop()
	{
		m_abort = true;

		LOG_DBG << "close all ws...";
		co_await close_all_ws();

		LOG_DBG << "database shutdown...";
		m_database.shutdown();

		LOG_DBG << "cmall_service.stop()";
	}

	boost::asio::awaitable<bool> cmall_service::load_configs()
	{
		std::vector<cmall_merchant> all_merchant;
		using query_t = odb::query<cmall_merchant>;
		auto query	  = query_t::deleted_at.is_null();
		co_await m_database.async_load<cmall_merchant>(query, all_merchant);

		for (cmall_merchant& merchant : all_merchant)
		{
			if (services::repo_products::is_git_repo(merchant.repo_path))
			{
				auto repo = std::make_shared<services::repo_products>(git_operation_thread_pool, merchant.uid_, merchant.repo_path);
				this->merchant_repos.emplace(merchant.uid_, repo);
			}
			else
			{
				LOG_ERR << "no bare git repos @(" << merchant.repo_path << ") for merchant <<" << merchant.name_;
			}
		}

		co_return true;
	}

	boost::asio::awaitable<void> cmall_service::run_httpd()
	{
		// 初始化ws acceptors.
		co_await init_ws_acceptors();

		constexpr int concurrent_accepter = 20;

		co_await httpd::detail::map(m_ws_acceptors, [concurrent_accepter](auto && a)mutable -> boost::asio::awaitable<void>{
			return a.run_accept_loop(concurrent_accepter);
		});
	}

	boost::asio::awaitable<bool> cmall_service::init_ws_acceptors()
	{
		boost::system::error_code ec;

		m_ws_acceptors.reserve(m_config.ws_listens_.size());

		for (const auto& wsd : m_config.ws_listens_)
		{
			if constexpr ( httpd::has_so_reuseport() )
			{
				for (int io_index = 0; io_index < m_io_context_pool.pool_size(); io_index++)
				{
					m_ws_acceptors.emplace_back(m_io_context_pool.get_io_context(io_index), *this);
					m_ws_acceptors.back().listen(wsd, ec);
					if (ec)
					{
						co_return false;
					}
				}
			}
			else
			{
				m_ws_acceptors.emplace_back(m_io_context, *this);
				m_ws_acceptors.back().listen(wsd, ec);
				if (ec)
				{
					co_return false;
				}
			}
		}

		co_return true;
	}

	// 从 git 仓库获取文件，没找到返回 0
	boost::asio::awaitable<int> cmall_service::render_git_repo_files(size_t connection_id, std::string merchant, std::string path_in_repo, boost::beast::tcp_stream& client, boost::beast::http::request<boost::beast::http::string_body>  req)
	{
		auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);

		if (!merchant_repos.contains(merchant_id))
			co_return 404;

		boost::system::error_code ec;
		auto res_body = co_await merchant_repos[merchant_id]->get_file_content(path_in_repo, ec);
		if (ec)
		{
			co_return 404;
		}

		ec = co_await httpd::send_string_response_body(client, res_body, http::make_http_last_modified(std::time(0) + 60),
					http::mime_map[boost::filesystem::path(path_in_repo).extension().string()], req.version(), req.keep_alive());
		if (ec)
			throw boost::system::system_error(ec);
		co_return 200;
	}

	// 成功给用户返回内容, 返回 200. 如果没找到商品, 不要向 client 写任何数据, 直接放回 404, 由调用方统一返回错误页面.
	boost::asio::awaitable<int> cmall_service::render_goods_detail_content(size_t connection_id, std::string merchant,
		std::string goods_id, boost::beast::tcp_stream& client, int http_ver, bool keepalive)
	{
		auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);
		if (merchant_repos.contains(merchant_id))
		{
			std::string product_detail = co_await merchant_repos[merchant_id]->get_product_detail(goods_id);

			auto ec = co_await httpd::send_string_response_body(client, product_detail, http::make_http_last_modified(std::time(0) + 60),
						"text/markdown; charset=utf-8", http_ver, keepalive);
			if (ec)
				throw boost::system::system_error(ec);
			co_return 200;
		}
		co_return 404;
	}

	boost::asio::awaitable<void> cmall_service::do_ws_read(size_t connection_id, client_connection_ptr connection_ptr)
	{
		while (!m_abort)
		{
			boost::beast::multi_buffer buffer{ 4 * 1024 * 1024 }; // max multi_buffer size 4M.
			co_await connection_ptr->ws_client->ws_stream_.async_read(buffer, boost::asio::use_awaitable);

			auto body = boost::beast::buffers_to_string(buffer.data());

			boost::system::error_code ec;
			auto jv	  = boost::json::parse(body, ec, {}, { 64, false, false, true });

			if (ec || !jv.is_object())
			{
				// 这里直接　co_return, 连接会关闭. 因为用户发来的数据连 json 都不是.
				// 对这种垃圾客户端，直接暴力断开不搭理.
				co_return;
			}

			maybe_jsonrpc_request_t maybe_req = boost::json::value_to<maybe_jsonrpc_request_t>(jv);
			if (!maybe_req.has_value())
			{
				// 格式不对.
				boost::json::object reply_message;
				if (jv.as_object().contains("id"))
					reply_message["id"]		 = jv.at("id");
				reply_message["error"]	 = { { "code", -32600 }, { "message", "Invalid Request" } };
				co_await websocket_write(*connection_ptr, jsutil::json_to_string(reply_message));
				continue;
			}

			auto req	= maybe_req.value();
			auto method = req.method;
			auto params = req.params;

			client_connection& this_client = *connection_ptr;

			if (!this_client.session_info)
			{
				if (method != "recover_session")
				{
					throw boost::system::system_error(boost::system::error_code(cmall::error::session_needed));
				}
				boost::json::object replay_message;
				// 未有 session 前， 先不并发处理 request，避免 客户端恶意并发 recover_session 把程序挂掉
				replay_message = co_await handle_jsonrpc_call(connection_ptr, method, params);
				replay_message.insert_or_assign("id", jv.at("id"));
				co_await websocket_write(*connection_ptr, jsutil::json_to_string(replay_message));
				continue;
			}

			// 每个请求都单开线程处理
			boost::asio::co_spawn(
				connection_ptr->get_executor(),
				[this, connection_ptr, method, params, jv]() -> boost::asio::awaitable<void>
				{
					boost::json::object replay_message;
					try
					{
						replay_message = co_await handle_jsonrpc_call(connection_ptr, method, params);
					}
					catch (boost::system::system_error& e)
					{
						replay_message["error"] = { { "code", e.code().value() }, { "message", e.code().message() } };
					}
					catch (std::exception& e)
					{
						LOG_ERR << e.what();
						replay_message["error"] = { { "code", 502 }, { "message", "internal server error" } };
					}
					// 每个请求都单开线程处理, 因此客户端收到的应答是乱序的,
					// 这就是 jsonrpc 里 id 字段的重要意义.
					// 将 id 字段原原本本的还回去, 客户端就可以根据 返回的 id 找到原来发的请求
					if (jv.as_object().contains("id"))
						replay_message.insert_or_assign("id", jv.at("id"));
					co_await websocket_write(*connection_ptr, jsutil::json_to_string(replay_message));
				},
				boost::asio::detached);
		}
	}

	boost::asio::awaitable<boost::json::object> cmall_service::handle_jsonrpc_call(
		client_connection_ptr connection_ptr, const std::string& methodstr, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;

		auto method = magic_enum::enum_cast<req_method>(methodstr);
		if (!method.has_value())
		{
			throw boost::system::system_error(boost::system::error_code(cmall::error::unknown_method));
		}

		auto ensure_login
			= [&](bool check_admin = false, bool check_merchant = false) mutable -> boost::asio::awaitable<void>
		{
			if (!this_client.session_info->user_info)
				throw boost::system::system_error(error::login_required);

			auto uid = this_client.session_info->user_info->uid_;
			if (check_admin)
			{
				// TODO, 检查用户是否是 admin 才能继续操作.
			}

			if (check_merchant)
			{
				cmall_merchant merchant;
				if (!co_await m_database.async_load<cmall_merchant>(uid, merchant))
					throw boost::system::system_error(error::merchant_user_required);
			}

			co_return;
		};

		if ((method.value() != req_method::recover_session))
		{
			if (!this_client.session_info)
				throw boost::system::system_error(boost::system::error_code(cmall::error::session_needed));
		}

		switch (method.value())
		{
			case req_method::recover_session:
			{
				std::string sessionid = jsutil::json_accessor(params).get_string("sessionid");

				if (sessionid.empty())
				{
					// 表示是第一次使用，所以创建一个新　session 给客户端.
					this_client.session_info = std::make_shared<services::client_session>();

					this_client.session_info->session_id = gen_uuid();
					sessionid							 = this_client.session_info->session_id;

					co_await session_cache_map.save(*this_client.session_info);
				}
				else if (!(co_await session_cache_map.exist(sessionid)))
				{
					// 表示session 过期，所以创建一个新　session 给客户端.
					this_client.session_info = std::make_shared<services::client_session>();

					this_client.session_info->session_id = gen_uuid();
					sessionid							 = this_client.session_info->session_id;

					co_await session_cache_map.save(*this_client.session_info);
				}
				else
				{
					this_client.session_info
						= std::make_shared<services::client_session>(co_await session_cache_map.load(sessionid));
				}

				if (this_client.session_info->user_info)
				{
					bool db_operation = co_await m_database.async_load<cmall_user>(
						this_client.session_info->user_info->uid_, *(this_client.session_info->user_info));
					if (!db_operation)
					{
						this_client.session_info->user_info = {};
					}
					else
					{
						std::unique_lock<std::shared_mutex> l(active_users_mtx);
						active_users.push_back(connection_ptr);
					}
				}

				reply_message["result"] = { { "session_id", this_client.session_info->session_id },
					{ "isLogin", static_cast<bool>(this_client.session_info->user_info) } };
			}
			break;

			case req_method::user_prelogin:
			case req_method::user_login:
			case req_method::user_islogin:
			case req_method::user_logout:
				co_return co_await handle_jsonrpc_user_api(connection_ptr, method.value(), params);
				break;
			case req_method::user_info:
			case req_method::user_apply_merchant:
			case req_method::user_list_recipient_address:
			case req_method::user_add_recipient_address:
			case req_method::user_modify_receipt_address:
			case req_method::user_erase_receipt_address:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_user_api(connection_ptr, method.value(), params);
				break;

			case req_method::cart_add:
			case req_method::cart_mod:
			case req_method::cart_del:
			case req_method::cart_list:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_cart_api(connection_ptr, method.value(), params);
				break;
			case req_method::order_create_cart:
			case req_method::order_create_direct:
			case req_method::order_status:
			case req_method::order_close:
			case req_method::order_list:
			case req_method::order_get_pay_url:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_order_api(connection_ptr, method.value(), params);
				break;

			case req_method::goods_list:
			{
				std::vector<services::product> all_products;

				// 列出 商品, 根据参数决定是首页还是商户
				auto merchant = jsutil::json_accessor(params).get_string("merchant");

				if (merchant == "")
				{
					for (auto& [merchant_id, merchant_repo] : merchant_repos)
					{
						std::vector<services::product> products = co_await merchant_repo->get_products();
						std::copy(products.begin(), products.end(), std::back_inserter(all_products));
					}

				}
				else
				{
					auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);
					if (merchant_repos.contains(merchant_id))
					{
						std::vector<services::product> products = co_await merchant_repos[merchant_id]->get_products();
						std::copy(products.begin(), products.end(), std::back_inserter(all_products));
					}
				}

				reply_message["result"] = boost::json::value_from(all_products);
			}
			break;
			case req_method::goods_detail:
			{
				auto merchant_id = jsutil::json_accessor(params).get("merchant_id", -1).as_int64();
				auto goods_id = jsutil::json_accessor(params).get_string("goods_id");

				if (!merchant_repos.contains(merchant_id))
					throw boost::system::system_error(cmall::error::invalid_params);
				if (goods_id.empty())
					throw boost::system::system_error(cmall::error::invalid_params);


				boost::system::error_code ec;
				auto product = co_await merchant_repos[merchant_id]->get_product(goods_id, ec);
				if (ec)
					throw boost::system::system_error(cmall::error::goods_not_found);
				// 获取商品信息, 注意这个不是商品描述, 而是商品 标题, 价格, 和缩略图. 用在商品列表页面.
				reply_message["result"] = boost::json::value_from(product);
			}
			break;
			case req_method::admin_user_list:
				break;
			case req_method::admin_user_ban:
				break;
			case req_method::admin_merchant_list:
				break;
			case req_method::admin_merchant_ban:
				break;
			case req_method::admin_product_list:
				break;
			case req_method::admin_product_withdraw:
				break;
			case req_method::admin_order_force_refund:
				break;
		}

		co_return reply_message;
	}

	boost::asio::awaitable<boost::json::object> cmall_service::handle_jsonrpc_user_api(
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
						if (co_await m_database.async_load<cmall_user>(query_t::active_phone == session_info.verify_telephone, user))
						{
							session_info.user_info = user;
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
						co_await session_cache_map.save(
							this_client.session_info->session_id, *this_client.session_info);
						reply_message["result"] = { { "login", "success" }, { "usertype", "user" } };

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
				this_client.session_info->user_info = {};
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
					// already applied
					throw boost::system::system_error(cmall::error::already_exist);
				}

				std::string name = jsutil::json_accessor(params).get_string("name");
				if (name.empty())
					throw boost::system::system_error(cmall::error::invalid_params);

				std::string desc = jsutil::json_accessor(params).get_string("desc");

				m.uid_ = uid;
				m.name_ = name;
				m.verified_ = false;
				if (!desc.empty())
					m.desc_ = desc;
				co_await m_database.async_add<cmall_merchant>(m);
				reply_message["result"] = true;
			}
			break;
			case req_method::user_list_recipient_address:
			{
				// 重新载入 user_info, 以便获取正确的收件人地址信息.
				co_await m_database.async_load<cmall_user>(session_info.user_info->uid_, *(session_info.user_info));
				cmall_user& user_info = *(session_info.user_info);
				boost::json::array recipients_array;
				for (int i = 0; i < user_info.recipients.size(); i++)
				{
					auto jsobj =  boost::json::value_from(user_info.recipients[i]);
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
				cmall_user& user_info = *(session_info.user_info);

				bool is_db_op_ok = co_await m_database.async_update<cmall_user>(user_info.uid_,
					[&](cmall_user&& value) {
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

	boost::asio::awaitable<boost::json::object> cmall_service::handle_jsonrpc_order_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;
		services::client_session& session_info = *this_client.session_info;
		cmall_user& this_user = *(session_info.user_info);

		switch (method)
		{
			case req_method::order_create_cart:
			case req_method::order_create_direct:
			{
				// 重新载入 user_info, 以便获取正确的收件人地址信息.
				co_await m_database.async_load<cmall_user>(this_user.uid_, *this_client.session_info->user_info);
				cmall_user& user_info = *(this_client.session_info->user_info);

				boost::json::array goods_array_ref	  = jsutil::json_accessor(params).get("goods", boost::json::array{}).as_array();

				auto recipient_id = jsutil::json_accessor(params).get("recipient_id", -1).as_int64();

				if (!(recipient_id >= 0 && recipient_id < (int64_t)user_info.recipients.size()))
				{
					throw boost::system::system_error(
						boost::system::error_code(cmall::error::recipient_id_out_of_range));
				}

				cmall_order new_order;
				new_order.recipient.push_back(user_info.recipients[recipient_id]);
				new_order.buyer_ = user_info.uid_;
				new_order.oid_	 = gen_uuid();
				new_order.stage_ = order_unpay;

				cpp_numeric total_price = 0;

				for (boost::json::value goods_v :  goods_array_ref)
				{
					boost::json::object goods_ref = goods_v.as_object();
					auto merchant_id_of_goods = goods_ref["merchant_id"].as_int64();
					auto goods_id_of_goods = jsutil::json_as_string(goods_ref["goods_id"].as_string(), "");

					boost::system::error_code ec;
					services::product product_in_mall = co_await merchant_repos[merchant_id_of_goods]->get_product(goods_id_of_goods, ec);
					if (ec) // 商品不存在
						continue;

					goods_snapshot good_snap;

					good_snap.description_ = product_in_mall.product_description;
					good_snap.good_version_git = product_in_mall.git_version;
					good_snap.name_ = product_in_mall.product_title;
					good_snap.merchant_id = merchant_id_of_goods;
					good_snap.goods_id = product_in_mall.product_id;
					good_snap.price_ = cpp_numeric(product_in_mall.product_price);
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
			case req_method::order_status:
				break;
			case req_method::order_close:
				break;
			case req_method::order_list:
			{
				auto page = jsutil::json_accessor(params).get("page", 0).as_int64();
				auto page_size = jsutil::json_accessor(params).get("page_size", 20).as_int64();

				std::vector<cmall_order> orders;
				using query_t = odb::query<cmall_order>;
				auto query	  = (query_t::buyer == this_user.uid_ && query_t::deleted_at.is_null()) + " order by "
					+ query_t::created_at + " desc limit " + std::to_string(page_size) + " offset "
					+ std::to_string(page * page_size);
				co_await m_database.async_load<cmall_order>(query, orders);

				LOG_DBG << "order_list retrieved, " << orders.size() << " items";
				reply_message["result"] = boost::json::value_from(orders);

			}break;
			case req_method::order_get_pay_url:
			{
				auto orderid	  = jsutil::json_accessor(params).get_string("orderid");
				auto payment = jsutil::json_accessor(params).get_string("payment");

				cmall_order order_to_pay;
				using query_t = odb::query<cmall_order>;
				auto query		   = query_t::oid == orderid && query_t::buyer == this_user.uid_;
				bool order_founded = co_await m_database.async_load<cmall_order>(query, order_to_pay);

				if (!order_founded)
				{
					throw boost::system::system_error(boost::system::error_code(cmall::error::order_not_found));
				}

				// 对已经存在的订单, 获取支付连接.
				services::payment_url payurl = co_await payment_service.get_payurl(orderid, 0, "", to_string(order_to_pay.price_), services::PAYMENT_CHSPAY);

				reply_message["result"] = { {"type", "url"}, {"url", payurl.uri } };
			}
			break;
			default:
				throw "this should never be executed";
		}
		co_return reply_message;
	}

	boost::asio::awaitable<boost::json::object> cmall_service::handle_jsonrpc_cart_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;
		services::client_session& session_info = *this_client.session_info;
		cmall_user& this_user = *(session_info.user_info);

		switch (method)
		{
			case req_method::cart_add: // 添加到购物车.
			{
				auto merchant_id = jsutil::json_accessor(params).get("merchant_id", -1).as_int64();
				auto goods_id = jsutil::json_accessor(params).get_string("goods_id");
				if (merchant_id < 0 || goods_id.empty())
					throw boost::system::system_error(cmall::error::invalid_params);

				cmall_cart item;
				using query_t = odb::query<cmall_cart>;
				auto query(query_t::uid == this_user.uid_ && query_t::merchant_id == merchant_id && query_t::goods_id == goods_id);
				if (co_await m_database.async_load<cmall_cart>(query, item))
					throw boost::system::system_error(cmall::error::already_in_cart);

				item.uid_ = this_user.uid_;
				item.merchant_id_ = merchant_id;
				item.goods_id_ = goods_id;
				item.count_ = 1;

				co_await m_database.async_add(item);
				reply_message["result"] = true;

				co_await send_notify_message(this_user.uid_, fmt::format(R"---({{"topic":"cart_changed", "session_id": "{}"}})---", this_client.session_info->session_id), this_client.connection_id_);
			}
			break;
			case req_method::cart_mod: // 修改数量.
			{
				auto item_id = jsutil::json_accessor(params).get("item_id", -1).as_int64();
				auto count = jsutil::json_accessor(params).get("count", 0).as_int64();
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

				co_await m_database.async_update<cmall_cart>(item_id, [count](cmall_cart&& old) mutable {
					old.count_ = count;
					return old;
				});
				reply_message["result"] = true;
				co_await send_notify_message(this_user.uid_, fmt::format(R"---({{"topic":"cart_changed", "session_id": "{}"}})---", this_client.session_info->session_id), this_client.connection_id_);
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
				co_await send_notify_message(this_user.uid_, fmt::format(R"---({{"topic":"cart_changed", "session_id": "{}"}})---", this_client.session_info->session_id), this_client.connection_id_);
			}
			break;
			case req_method::cart_list: // 查看购物车列表.
			{
				auto page = jsutil::json_accessor(params).get("page", 0).as_int64();
				auto page_size = jsutil::json_accessor(params).get("page_size", 20).as_int64();

				std::vector<cmall_cart> items;
				using query_t = odb::query<cmall_cart>;
				auto query	  = (query_t::uid == this_user.uid_) + " order by " + query_t::created_at + " desc limit "
					+ std::to_string(page_size) + " offset " + std::to_string(page * page_size);
				co_await m_database.async_load<cmall_cart>(query, items);

				reply_message["result"] = boost::json::value_from(items);
			}
			break;
			default:
				throw "this should never be executed";
		}
		co_return reply_message;
	}

	boost::asio::awaitable<void> cmall_service::send_notify_message(std::uint64_t uid_, const std::string& msg, std::int64_t exclude_connection_id)
	{
		std::vector<client_connection_ptr> active_user_connections;
		{
			std::shared_lock<std::shared_mutex> l(active_users_mtx);
			for (auto c : boost::make_iterator_range(active_users.get<2>().equal_range(uid_)))
			{
				active_user_connections.push_back(c);
			}
		}
		for (auto c : active_user_connections)
			co_await websocket_write(*c, msg);

	}

	boost::asio::awaitable<void> cmall_service::do_ws_write(size_t connection_id, client_connection_ptr connection_ptr)
	{
		auto& message_deque = connection_ptr->ws_client->message_channel;
		auto& ws			= connection_ptr->ws_client->ws_stream_;

		using namespace boost::asio::experimental::awaitable_operators;

		while (!m_abort)
			try
			{
				timer t(co_await boost::asio::this_coro::executor);
				t.expires_from_now(std::chrono::seconds(15));
				std::variant<std::monostate, std::string> awaited_result
					= co_await(t.async_wait(boost::asio::use_awaitable)
						|| message_deque.async_receive(boost::asio::use_awaitable));
				if (awaited_result.index() == 0)
				{
					LOG_DBG << "coro: do_ws_write: [" << connection_id << "], send ping to client";
					co_await ws.async_ping("", boost::asio::use_awaitable); // timed out
				}
				else
				{
					auto message = std::get<1>(awaited_result);
					if (message.empty())
						co_return;
					co_await ws.async_write(boost::asio::buffer(message), boost::asio::use_awaitable);
				}
			}
			catch (boost::system::system_error& e)
			{
				boost::system::error_code ec = e.code();
				connection_ptr->tcp_stream.close();
				LOG_ERR << "coro: do_ws_write: [" << connection_id << "], exception:" << ec.message();
				co_return;
			}
	}

	boost::asio::awaitable<void> cmall_service::close_all_ws()
	{
		co_await httpd::detail::map(m_ws_acceptors, [](auto&& a) mutable -> boost::asio::awaitable<void>{
			co_return co_await a.clean_shutdown();
		});

		LOG_DBG << "cmall_service::close_all_ws() success!";
	}

	boost::asio::awaitable<void> cmall_service::websocket_write(client_connection& connection_, std::string message)
	{
		if (connection_.ws_client)
		{
			co_await boost::asio::co_spawn(connection_.get_executor(),
				[&connection_, message]() mutable -> boost::asio::awaitable<void>
				{ connection_.ws_client->message_channel.try_send(boost::system::error_code(), message); co_return; }, boost::asio::use_awaitable);
		}
	}

	client_connection_ptr cmall_service::make_shared_connection(const boost::asio::any_io_executor& io, std::int64_t connection_id)
	{
		if constexpr (httpd::has_so_reuseport())
			return std::make_shared<client_connection>(io, connection_id);
		else
			return std::make_shared<client_connection>(m_io_context_pool.get_io_context(), connection_id);
	}

	boost::asio::awaitable<void> cmall_service::client_connected(client_connection_ptr client_ptr)
	{
		using string_body = boost::beast::http::string_body;
		using fields	  = boost::beast::http::fields;
		using request	  = boost::beast::http::request<string_body>;
		using response	  = boost::beast::http::response<string_body>;

		const size_t connection_id = client_ptr->connection_id_;

		bool keep_alive = false;

		auto http_simple_error_page
			= [&client_ptr](auto body, auto status_code, unsigned version) mutable -> boost::asio::awaitable<void>
		{
			response res{ static_cast<boost::beast::http::status>(status_code), version };
			res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
			res.set(boost::beast::http::field::content_type, "text/html");
			res.keep_alive(false);
			res.body() = body;
			res.prepare_payload();

			boost::beast::http::serializer<false, string_body, fields> sr{ res };
			co_await boost::beast::http::async_write(client_ptr->tcp_stream, sr, boost::asio::use_awaitable);
			client_ptr->tcp_stream.close();
		};

		LOG_DBG << "coro created: handle_accepted_client( " << connection_id << ")";

		do
		{
			boost::beast::flat_buffer buffer;
			request req;

			co_await boost::beast::http::async_read(client_ptr->tcp_stream, buffer, req, boost::asio::use_awaitable);
			boost::string_view target = req.target();

			LOG_DBG << "coro: handle_accepted_client: [" << connection_id << "], got request on " << target.to_string();

			if (target.empty() || target[0] != '/' || target.find("..") != boost::beast::string_view::npos)
			{
				co_await http_simple_error_page(
					"Illegal request-target", boost::beast::http::status::bad_request, req.version());
			}
			// 处理 HTTP 请求.
			else if (boost::beast::websocket::is_upgrade(req))
			{
				LOG_DBG << "ws client incoming: " << connection_id << ", remote: " << client_ptr->remote_host_;

				if (!target.starts_with("/api"))
				{
					co_await http_simple_error_page(
						"not allowed", boost::beast::http::status::forbidden, req.version());
					break;
				}

				client_ptr->ws_client.emplace(client_ptr->tcp_stream);

				client_ptr->ws_client->ws_stream_.set_option(boost::beast::websocket::stream_base::decorator(
					[](auto& res) { res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING); }));

				co_await client_ptr->ws_client->ws_stream_.async_accept(req, boost::asio::use_awaitable);

				// 获取executor.
				auto executor = client_ptr->tcp_stream.get_executor();

				// 接收到pong, 重置超时定时器.
				client_ptr->ws_client->ws_stream_.control_callback(
					[&tcp_stream = client_ptr->tcp_stream](
						boost::beast::websocket::frame_type ft, boost::beast::string_view)
					{
						if (ft == boost::beast::websocket::frame_type::pong)
							tcp_stream.expires_after(std::chrono::seconds(60));
					});

				using namespace boost::asio::experimental::awaitable_operators;

				co_await(
					// 启动读写协程.
					do_ws_read(connection_id, client_ptr) && do_ws_write(connection_id, client_ptr));
				LOG_DBG << "handle_accepted_client, " << connection_id << ", connection exit.";
				co_return;
			}
			else
			{
				if (target.starts_with("/api"))
				{
					co_await http_simple_error_page(
						"not allowed", boost::beast::http::status::forbidden, req.version());
					continue;
				}

				if (target.starts_with("/repos"))
				{
					boost::match_results<boost::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/((images|css)/.+)")))
					{
						std::string merhcant = w[1].str();
						std::string remains = w[2].str();

						int status_code = co_await render_git_repo_files(connection_id, merhcant, remains, client_ptr->tcp_stream, req);

						if (status_code != 200)
						{
							co_await http_simple_error_page("ERRORED", status_code, req.version());
						}
					}
					else
					{
						co_await http_simple_error_page("ERRORED", 403, req.version());
					}
					continue;

				}

				// 这个 /goods/${merchant}/${goods_id} 获取 富文本的商品描述.
				if (target.starts_with("/goods"))
				{
					boost::match_results<boost::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/goods/([^/]+)/([^/]+)")))
					{
						std::string merhcant = w[1].str();
						std::string goods_id = w[2].str();

						int status_code = co_await render_goods_detail_content(
							connection_id, merhcant, goods_id, client_ptr->tcp_stream, req.version(), req.keep_alive());

						if (status_code != 200)
						{
							co_await http_simple_error_page("ERRORED", status_code, req.version());
						}
					}
					else
					{
						co_await http_simple_error_page("access denied", 401, req.version());
					}
					continue;
				}

				// 这里使用 zip 里打包的 angular 页面. 对不存在的地址其实直接替代性的返回 index.html 因此此
				// api 绝对不返回 400. 如果解压内部 zip 发生错误, 会放回 500 错误代码.
				int status_code = co_await http_handle_static_file(connection_id, req, client_ptr->tcp_stream);

				if (status_code != 200)
				{
					co_await http_simple_error_page("internal server error", status_code, req.version());
				}

				keep_alive = !req.need_eof();
			}
		} while (keep_alive && (!m_abort));

		LOG_DBG << "handle_accepted_client: HTTP connection closed : " << connection_id;
	}

	boost::asio::awaitable<void> cmall_service::client_disconnected(client_connection_ptr c)
	{
		std::unique_lock<std::shared_mutex> l(active_users_mtx);
		active_users.get<1>().erase(c->connection_id_);
		co_return;
	}

}
