
#include "stdafx.hpp"

#include "cmall/error_code.hpp"
#include "cmall/misc.hpp"

#include "cmall/cmall.hpp"
#include "cmall/database.hpp"
#include "cmall/db.hpp"

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4244 4459)
#endif // _MSC_VER

#include "cmall/http_static_file_handle.hpp"
#include "cmall/js_util.hpp"

#include "services/repo_products.hpp"

#include "cmall/conversion.hpp"

#include "httpd/http_misc_helper.hpp"
#include "httpd/httpd.hpp"
#include "httpd/wait_all.hpp"
#include "dirmon/dirmon.hpp"

namespace cmall
{
	using namespace std::chrono_literals;

	//////////////////////////////////////////////////////////////////////////

	cmall_service::cmall_service(io_context_pool& ios, const server_config& config)
		: m_io_context_pool(ios)
		, m_io_context(m_io_context_pool.server_io_context())
		, m_config(config)
		, m_database(m_config.dbcfg_)
		, background_task_thread_pool(8)
		, session_cache_map(m_config.session_cache_file)
		, telephone_verifier(m_io_context)
		, payment_service(m_io_context)
		, script_runner(m_io_context)
		, gitea_service(m_io_context)
		, sslctx_(boost::asio::ssl::context::tls_server)
	{
	}

	cmall_service::~cmall_service() { LOG_DBG << "~cmall_service()"; }

	awaitable<void> cmall_service::stop()
	{
		m_abort = true;

		m_abort_signal.emit(boost::asio::cancellation_type::all);

		LOG_DBG << "close all ws...";
		co_await close_all_ws();

		LOG_DBG << "database shutdown...";
		m_database.shutdown();

		LOG_DBG << "cmall_service.stop()";
	}

	// false if no git repo for merchant
	awaitable<bool> cmall_service::load_merchant_git(const cmall_merchant& merchant)
	{
		if (services::repo_products::is_git_repo(merchant.repo_path))
		{
			auto repo = std::make_shared<services::repo_products>(
				background_task_thread_pool, merchant.uid_, merchant.repo_path);
			std::unique_lock<std::shared_mutex> l(merchant_repos_mtx);
			merchant_repos.get<tag::merchant_uid_tag>().erase(merchant.uid_);
			merchant_repos.get<tag::merchant_uid_tag>().insert(repo);
			boost::asio::co_spawn(background_task_thread_pool, [this, repo]() mutable -> awaitable<void>
			{
				co_await search_service.add_merchant(repo);
				co_await repo_push_check(std::move(repo));
			}, boost::asio::bind_cancellation_slot(m_abort_signal.slot(), boost::asio::detached));
			co_return true;
		}
		else
		{
			std::unique_lock<std::shared_mutex> l(merchant_repos_mtx);
			merchant_repos.get<tag::merchant_uid_tag>().erase(merchant.uid_);
			LOG_ERR << "no bare git repos @(" << merchant.repo_path << ") for merchant <<" << merchant.name_;
		}
		co_return false;
	}

	std::shared_ptr<services::repo_products> cmall_service::get_merchant_git_repo(std::uint64_t merchant_uid, boost::system::error_code& ec) const
	{
		std::shared_lock<std::shared_mutex> l(merchant_repos_mtx);
		auto& index_by_uid = merchant_repos.get<tag::merchant_uid_tag>();
		auto it = index_by_uid.find(merchant_uid);
		if (it == index_by_uid.end())
		{
			ec = cmall::error::merchant_vanished;
			return {};
		}
		return * it;
	}

	std::shared_ptr<services::repo_products> cmall_service::get_merchant_git_repo(const cmall_merchant& merchant, boost::system::error_code& ec) const
	{
		return get_merchant_git_repo(merchant.uid_, ec);
	}

	 // will throw if not found
	std::shared_ptr<services::repo_products> cmall_service::get_merchant_git_repo(const cmall_merchant& merchant) const
	{
		return get_merchant_git_repo(merchant.uid_);
	}

	 // will throw if not found
	std::shared_ptr<services::repo_products> cmall_service::get_merchant_git_repo(std::uint64_t merchant_uid) const
	{
		std::shared_lock<std::shared_mutex> l(merchant_repos_mtx);
		auto& index_by_uid = merchant_repos.get<tag::merchant_uid_tag>();
		auto it = index_by_uid.find(merchant_uid);
		if (it == index_by_uid.end())
		{
			throw boost::system::system_error(cmall::error::merchant_vanished);
		}
		return * it;
	}

	awaitable<void> cmall_service::repo_push_check(std::weak_ptr<services::repo_products> repo_)
	{
		std::string repo_dir;

		{
			auto repo = repo_.lock();
			if (!repo)
				co_return;
			repo_dir = repo->repo_path().string();
		}

		dirmon::dirmon git_monitor(co_await boost::asio::this_coro::executor, repo_dir);

		boost::asio::cancellation_state cs = co_await boost::asio::this_coro::cancellation_state;
		if (m_abort)
			co_return;

		if (cs.slot().is_connected())
		{
			cs.slot().assign([&git_monitor](boost::asio::cancellation_type_t) mutable
			{
				boost::system::error_code ignore_ec;
				git_monitor.close(ignore_ec);
			});
		}

		while (!m_abort)
		{
			co_await git_monitor.async_wait_dirchange();

			while(!m_abort)
			{
				steady_timer timer(co_await boost::asio::this_coro::executor);
				timer.expires_from_now(1s);

				using namespace boost::asio::experimental::awaitable_operators;

				auto awaited_result = co_await (
					timer.async_wait(use_awaitable)
						||
					git_monitor.async_wait_dirchange()
				);

				if (m_abort) co_return;

				// 只有持续 1s 左右的时间 git 仓库没有产生 inotify 消息，才认为 git 完成了动作.
				// 不然就继续读取 inotify 获取消息.

				if (awaited_result.index() == 0)
				{
					break;
				}
			}

			if (m_abort) co_return;

			auto repo = repo_.lock();
			if (!repo)
				co_return;
			{
				if (co_await repo->check_repo_changed())
				{
					LOG_DBG << std::format("repo {} git HEAD changed!", repo->repo_path().string());
					if (m_abort) co_return;
					co_await search_service.reload_merchant(repo);
				}
			}
		}
	}

	awaitable<bool> cmall_service::load_repos()
	{
		std::vector<cmall_merchant> all_merchant;
		using query_t = odb::query<cmall_merchant>;
		auto query	  = query_t::deleted_at.is_null();
		co_await m_database.async_load<cmall_merchant>(query, all_merchant);

		for (cmall_merchant& merchant : all_merchant)
		{
			co_await load_merchant_git(merchant);
		}

		co_return true;
	}

	awaitable<void> cmall_service::run_httpd()
	{
		constexpr int concurrent_accepter = 20;

		std::vector<promise<void(std::exception_ptr)>> co_threads;

		for (auto&& a: m_ws_acceptors)
        {
			co_threads.push_back(boost::asio::co_spawn(a.get_executor(), a.run_accept_loop(concurrent_accepter), use_promise));
        }

		for (auto&& a: m_wss_acceptors)
        {
			co_threads.push_back(boost::asio::co_spawn(a.get_executor(), a.run_accept_loop(concurrent_accepter), use_promise));
        }

		for (auto&& a: m_ws_unix_acceptors)
        {
			co_threads.push_back(boost::asio::co_spawn(a.get_executor(), a.run_accept_loop(concurrent_accepter), use_promise));
        }

        // 然后等待所有的协程工作完毕.
        for (auto&& co : co_threads)
            co_await co.async_wait(use_awaitable);
	}

	awaitable<bool> cmall_service::init_ws_acceptors()
	{
		boost::system::error_code ec;

		m_ws_acceptors.reserve(m_config.ws_listens_.size());

		for (const auto& wsd : m_config.ws_listens_)
		{
			if constexpr (httpd::has_so_reuseport())
			{
				for (std::size_t io_index = 0; io_index < m_io_context_pool.pool_size(); io_index++)
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

	awaitable<bool> cmall_service::init_wss_acceptors(std::string_view cert, std::string_view key)
	{
		sslctx_.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2);

		sslctx_.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));

		sslctx_.use_private_key(
			boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);

//		sslctx_.use_tmp_dh(boost::asio::buffer(dh.data(), dh.size()));


		boost::system::error_code ec;

		m_wss_acceptors.reserve(m_config.wss_listens_.size());

		for (const auto& wsd : m_config.wss_listens_)
		{
			if constexpr (httpd::has_so_reuseport())
			{
				for (std::size_t io_index = 0; io_index < m_io_context_pool.pool_size(); io_index++)
				{
					m_wss_acceptors.emplace_back(m_io_context_pool.get_io_context(io_index), sslctx_, *this);
					m_wss_acceptors.back().listen(wsd, ec);
					if (ec)
					{
						co_return false;
					}
				}
			}
			else
			{
				m_wss_acceptors.emplace_back(m_io_context, sslctx_, *this);
				m_wss_acceptors.back().listen(wsd, ec);
				if (ec)
				{
					co_return false;
				}
			}
		}

		co_return true;
	}

	awaitable<bool> cmall_service::init_ws_unix_acceptors()
	{
		boost::system::error_code ec;

		m_ws_unix_acceptors.reserve(m_config.ws_unix_listens_.size());

		for (const auto& wsd : m_config.ws_unix_listens_)
		{
			m_ws_unix_acceptors.emplace_back(m_io_context, *this);
			m_ws_unix_acceptors.back().listen(wsd, ec);
			if (ec)
			{
				co_return false;
			}
		}

		co_return true;
	}

	// 从 git 仓库获取文件，没找到返回 0
	awaitable<int> cmall_service::render_git_repo_files(size_t connection_id, std::string merchant,
		std::string path_in_repo, httpd::http_any_stream& client,
		boost::beast::http::request<boost::beast::http::string_body> req)
	{
		auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);

		boost::system::error_code ec;
		auto merchant_repo_ptr = get_merchant_git_repo(merchant_id, ec);

		if (ec)
			co_return 404;

		auto res_body = co_await merchant_repo_ptr->get_file_content(path_in_repo, ec);
		if (ec)
		{
			co_return 404;
		}

		ec = co_await httpd::send_string_response_body(client,
			res_body,
			httpd::make_http_last_modified(std::time(0) + 60),
			httpd::get_mime_type_from_extension(std::filesystem::path(path_in_repo).extension().string()),
			req.version(),
			req.keep_alive());
		if (ec)
			throw boost::system::system_error(ec);
		co_return 200;
	}

	// 成功给用户返回内容, 返回 200. 如果没找到商品, 不要向 client 写任何数据, 直接放回 404, 由调用方统一返回错误页面.
	awaitable<int> cmall_service::render_goods_detail_content(size_t connection_id, std::string merchant,
		std::string goods_id, httpd::http_any_stream& client, int http_ver, bool keepalive)
	{
		auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);
		boost::system::error_code ec;
		auto merchant_repo_ptr = get_merchant_git_repo(merchant_id, ec);

		if (ec)
			co_return 404;

		std::string product_detail = co_await merchant_repo_ptr->get_product_detail(goods_id);

		ec = co_await httpd::send_string_response_body(client,
			product_detail,
			httpd::make_http_last_modified(std::time(0) + 60),
			"text/markdown; charset=utf-8",
			http_ver,
			keepalive);
		if (ec)
			throw boost::system::system_error(ec);
		co_return 200;
	}

	awaitable<void> cmall_service::do_ws_read(size_t connection_id, client_connection_ptr connection_ptr)
	{
		while (!m_abort)
		{
			boost::beast::multi_buffer buffer{ 4 * 1024 * 1024 }; // max multi_buffer size 4M.
			co_await connection_ptr->ws_client->ws_stream_.async_read(buffer, use_awaitable);

			auto body = boost::beast::buffers_to_string(buffer.data());

			boost::system::error_code ec;
			auto jv = boost::json::parse(body, ec, {}, { 64, false, false, true });

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
					reply_message["id"] = jv.at("id");
				reply_message["error"] = { { "code", -32600 }, { "message", "Invalid Request" } };
				co_await websocket_write(connection_ptr, jsutil::json_to_string(reply_message)).async_wait(use_awaitable);
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
					throw boost::system::system_error(cmall::error::session_needed);
				}
				boost::json::object replay_message;
				// 未有 session 前， 先不并发处理 request，避免 客户端恶意并发 recover_session 把程序挂掉
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
				if (jv.as_object().contains("id"))
					replay_message.insert_or_assign("id", jv.at("id"));
				co_await websocket_write(connection_ptr, jsutil::json_to_string(replay_message)).async_wait(use_awaitable);
				continue;
			}

			// 每个请求都单开线程处理
			boost::asio::co_spawn(
				connection_ptr->get_executor(),
				[this, connection_ptr, method, params, jv]() -> awaitable<void>
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
					co_await websocket_write(connection_ptr, jsutil::json_to_string(replay_message)).async_wait(use_awaitable);
				},
				boost::asio::detached);
		}
	}

	awaitable<boost::json::object> cmall_service::handle_jsonrpc_call(
		client_connection_ptr connection_ptr, const std::string& methodstr, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;

		auto method = magic_enum::enum_cast<req_method>(methodstr);
		if (!method.has_value())
		{
			throw boost::system::system_error(cmall::error::unknown_method);
		}

		auto ensure_login
			= [&](bool check_admin = false, bool check_merchant = false) mutable -> awaitable<void>
		{
			if (!this_client.session_info->user_info)
				throw boost::system::system_error(error::login_required);

			if (check_admin)
			{
				if (!this_client.session_info->isAdmin)
					throw boost::system::system_error(error::admin_user_required);
			}

			if (check_merchant)
			{
				if (!this_client.session_info->isMerchant)
					throw boost::system::system_error(error::merchant_user_required);
			}

			co_return;
		};

		if ((method.value() != req_method::recover_session))
		{
			if (!this_client.session_info)
				throw boost::system::system_error(cmall::error::session_needed);
		}

		switch (method.value())
		{
			case req_method::recover_session:
			{
				std::string sessionid = jsutil::json_accessor(params).get_string("sessionid");
				std::string api_token = jsutil::json_accessor(params).get_string("api-token");

				if (api_token.empty())
				{
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
							cmall_merchant merchant_user;
							administrators admin_user;
							// 如果是 merchant/admin 也载入他们的信息
							if (co_await m_database.async_load<cmall_merchant>(this_client.session_info->user_info->uid_, merchant_user))
							{
								this_client.session_info->merchant_info = merchant_user;
								this_client.session_info->isMerchant = true;
							}
							if (co_await m_database.async_load<administrators>(this_client.session_info->user_info->uid_, admin_user))
							{
								this_client.session_info->admin_info = admin_user;
								this_client.session_info->isAdmin = true;
							}

							std::unique_lock<std::shared_mutex> l(active_users_mtx);
							active_users.push_back(connection_ptr);
						}
					}

					reply_message["result"] = {
						{ "session_id", this_client.session_info->session_id },
						{ "isAdmin", static_cast<bool>(this_client.session_info->isAdmin) },
						{ "isMerchant", static_cast<bool>(this_client.session_info->isMerchant) },
						{ "isLogin", static_cast<bool>(this_client.session_info->user_info) },
					};
				}
				else
				{
					cmall_apptoken appid_info;
					if (co_await m_database.async_load<cmall_apptoken>(odb::query<cmall_apptoken>::apptoken == api_token, appid_info))
					{
						this_client.session_info = std::make_shared<services::client_session>();
						cmall_user db_user;
						if (co_await m_database.async_load<cmall_user>(appid_info.uid_, db_user))
						{
							this_client.session_info->user_info = db_user;
						}
						else
						{
							reply_message["result"] = { { "login", "failed" } };
							break;
						}
						cmall_merchant db_merchant;
						if (co_await m_database.async_load<cmall_merchant>(appid_info.uid_, db_merchant))
						{
							this_client.session_info->isMerchant = true;
							this_client.session_info->merchant_info = db_merchant;
						}
						administrators db_admin;
						if (co_await m_database.async_load<administrators>(appid_info.uid_, db_admin))
						{
							this_client.session_info->isAdmin = true;
							this_client.session_info->admin_info = db_admin;
						}
						reply_message["result"] = {
							{ "login", "success" },
							{ "isMerchant", this_client.session_info->isMerchant },
							{ "isAdmin", this_client.session_info->isAdmin },
						};
					}
				}
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
			case req_method::user_apply_info:
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
			case req_method::order_detail:
			case req_method::order_close:
			case req_method::order_list:
			case req_method::order_get_pay_url:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_order_api(connection_ptr, method.value(), params);
				break;

			case req_method::search_goods:
			case req_method::goods_list:
			case req_method::goods_detail:
				co_return co_await handle_jsonrpc_goods_api(connection_ptr, method.value(), params);
				break;
			case req_method::merchant_info:
			case req_method::merchant_get_sold_order_detail:
			case req_method::merchant_sold_orders_mark_payed:
			case req_method::merchant_goods_list:
			case req_method::merchant_list_sold_orders:
			case req_method::merchant_sold_orders_add_kuaidi:
			case req_method::merchant_delete_sold_orders:
			case req_method::merchant_get_gitea_password:
			case req_method::merchant_reset_gitea_password:
				co_await ensure_login(false, true);
				co_return co_await handle_jsonrpc_merchant_api(connection_ptr, method.value(), params);
				break;
			case req_method::admin_user_detail:
			case req_method::admin_user_list:
			case req_method::admin_user_ban:
			case req_method::admin_list_applicants:
			case req_method::admin_approve_merchant:
			case req_method::admin_deny_applicant:
			case req_method::admin_list_merchants:
			case req_method::admin_disable_merchants:
			case req_method::admin_reenable_merchants:
				co_await ensure_login(true);
				co_return co_await handle_jsonrpc_admin_api(connection_ptr, method.value(), params);

				break;
		}

		co_return reply_message;
	}

	awaitable<boost::json::object> cmall_service::handle_jsonrpc_user_api(
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
									session_info.isMerchant = true;
								}
							}
							if (co_await m_database.async_load<administrators>(user.uid_, admin_user))
							{
								session_info.admin_info = admin_user;
								session_info.isAdmin = true;
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
						co_await session_cache_map.save(
							this_client.session_info->session_id, *this_client.session_info);
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
					throw boost::system::system_error(cmall::error::already_exist);
				}

				cmall_apply_for_mechant apply;
				apply.applicant_ = std::make_shared<cmall_user>(this_client.session_info->user_info.value());
				co_await m_database.async_add(apply);

				reply_message["result"] = true;
			}
			break;
			case req_method::user_apply_info:
			{
				auto uid = session_info.user_info->uid_;

				std::vector<cmall_apply_for_mechant> applys;
				using query_t = odb::query<cmall_apply_for_mechant>;
				auto query = (query_t::applicant->uid == uid && query_t::deleted_at.is_null()) + " order by " + query_t::created_at + " desc";
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

	awaitable<boost::json::object> cmall_service::handle_jsonrpc_order_api(
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

	awaitable<boost::json::object> cmall_service::handle_jsonrpc_cart_api(
		client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
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

	awaitable<boost::json::object> cmall_service::handle_jsonrpc_goods_api(
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
				auto q	 = jsutil::json_accessor(params).get_string("q");
				auto search_result = co_await search_service.search_goods(q);

				// then transform goods_ref to products
				std::vector<services::product> final_result;
				for (goods_ref gr : search_result)
				{
					final_result.push_back(
						co_await get_merchant_git_repo(gr.merchant_id)->get_product(gr.goods_id)
					);
				}

				reply_message["result"] = boost::json::value_from(final_result);
			}break;
			case req_method::goods_list:
			{
				// 列出 商品, 根据参数决定是首页还是商户
				auto merchant	 = jsutil::json_accessor(params).get_string("merchant");
				auto merchant_id = parse_number<std::uint64_t>(merchant);

				std::vector<cmall_merchant> merchants;
				using query_t = odb::query<cmall_merchant>;
				auto query	  = (query_t::state == merchant_state_t::normal)
					&& (query_t::deleted_at.is_null());
				if (merchant_id.has_value())
				{
					query = query_t::uid == merchant_id.value() && query;
				}
				query += " order by uid desc"; // 优先显示新商户的商品.

				co_await m_database.async_load<cmall_merchant>(query, merchants);

				std::vector<services::product> all_products;
				if (merchants.size() > 0)
				{
					for (const auto& m : merchants)
					{
						if (!services::repo_products::is_git_repo(m.repo_path))
							continue;

						auto repo
							= std::make_shared<services::repo_products>(background_task_thread_pool, m.uid_, m.repo_path);
						std::vector<services::product> products = co_await repo->get_products();
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

				auto product = co_await get_merchant_git_repo(merchant_id)->get_product(goods_id);

				// 获取商品信息, 注意这个不是商品描述, 而是商品 标题, 价格, 和缩略图. 用在商品列表页面.
				reply_message["result"] = boost::json::value_from(product);
			}
			catch(std::invalid_argument&)
			{
				throw boost::system::system_error(error::invalid_params);
			}
			break;
			default:
				throw "this should never be executed";
		}

		co_return reply_message;
	}

	awaitable<boost::json::object> cmall_service::handle_jsonrpc_merchant_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;
		services::client_session& session_info = *this_client.session_info;
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

				this_client.session_info->merchant_info = m;

				reply_message["result"] = boost::json::value_from(m);
			}
			break;
			case req_method::merchant_get_sold_order_detail:
			{
				auto orderid	   = jsutil::json_accessor(params).get_string("orderid");

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
			case req_method::merchant_sold_orders_mark_payed:
			{
				auto orderid	   = jsutil::json_accessor(params).get_string("orderid");

				std::vector<cmall_order> orders;
				using query_t = odb::query<cmall_order>;
				auto query	  = (query_t::oid == orderid && query_t::seller == this_user.uid_ && query_t::deleted_at.is_null());
				co_await m_database.async_load<cmall_order>(query, orders);

				LOG_DBG << "order_list retrieved, " << orders.size() << " items";
				if (orders.size() == 1)
				{
					if (orders[0].payed_at_.null())
					{
						orders[0].payed_at_ = boost::posix_time::second_clock::local_time();

						bool success = co_await m_database.async_update<cmall_order>(orders[0]);
						reply_message["result"] = success;

						if (success)
						{
							boost::system::error_code ec;
							auto merchant_repo_ptr = get_merchant_git_repo(this_merchant, ec);
							if (merchant_repo_ptr)
							{
								std::string pay_script_content = co_await merchant_repo_ptr->get_file_content("scripts/orderstatus.js", ec);
								if (!ec && !pay_script_content.empty())
								{
									boost::asio::co_spawn(background_task_thread_pool,
									 script_runner.run_script(pay_script_content, {"--order-id", orderid}), boost::asio::detached);
								}
							}
						}
						else
						{
							throw boost::system::system_error(cmall::error::internal_server_error);
						}
					}
					reply_message["result"] = true;
				}
				else
					throw boost::system::system_error(cmall::error::order_not_found);
			}
			break;
			case req_method::merchant_goods_list:
			{
				// 列出 商品, 根据参数决定是首页还是商户
				std::vector<services::product> all_products = co_await get_merchant_git_repo(this_merchant)->get_products();
				reply_message["result"] = boost::json::value_from(all_products);

			}break;
			case req_method::merchant_list_sold_orders:
			{
				std::vector<cmall_order> orders;
				using query_t = odb::query<cmall_order>;
				auto query	  = (query_t::seller == this_user.uid_ && query_t::deleted_at.is_null()) + " order by " + query_t::created_at + " desc";
				co_await m_database.async_load<cmall_order>(query, orders);

				LOG_DBG << "order_list retrieved, " << orders.size() << " items";
				reply_message["result"] = boost::json::value_from(orders);
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
			default:
				throw "this should never be executed";
		}

		co_return reply_message;
	}

	awaitable<boost::json::object> cmall_service::handle_jsonrpc_admin_api(client_connection_ptr connection_ptr, const req_method method, boost::json::object params)
	{
		client_connection& this_client = *connection_ptr;
		boost::json::object reply_message;
		services::client_session& session_info = *this_client.session_info;
		cmall_user& this_user				   = *(session_info.user_info);
		boost::ignore_unused(this_user);

		switch (method)
		{
			case req_method::admin_user_list:
			case req_method::admin_user_ban:
				break;
			case req_method::admin_user_detail:
			{
				// TODO

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
				auto query = " order by " + query_t::created_at + " desc";
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

				bool succeed = co_await m_database.async_transacton([&](const cmall_database::odb_transaction_ptr& tx) mutable -> awaitable<bool> {
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
					co_await boost::asio::co_spawn(background_task_thread_pool, [this, m]() mutable -> awaitable<void>
					{
						// 初始化仓库.
						std::string gitea_template_loaction = m_config.gitea_template_location.string();
						co_await gitea_service.init_user(m.uid_, m.gitea_password.get(), gitea_template_loaction);
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
				co_await m_database.async_update<cmall_apply_for_mechant>(query, [reason](cmall_apply_for_mechant&& apply) mutable {
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
					co_await m_database.async_update<cmall_merchant>(query, [state](cmall_merchant&& m) mutable {
						m.state_ = state;
						return m;
					});
				}
				reply_message["result"] = true;
			}
			break;
			default:
				throw "this should never be executed";
		}

		co_return reply_message;
	}

	awaitable<void> cmall_service::send_notify_message(
		std::uint64_t uid_, const std::string& msg, std::int64_t exclude_connection_id)
	{
		std::vector<client_connection_ptr> active_user_connections;
		{
			std::shared_lock<std::shared_mutex> l(active_users_mtx);
			for (auto c : boost::make_iterator_range(active_users.get<2>().equal_range(uid_)))
			{
				active_user_connections.push_back(c);
			}
		}
		std::vector<promise<void(std::exception_ptr)>> writers;
		for (auto c : active_user_connections)
			 writers.push_back(websocket_write(c, msg));
		co_await httpd::wait_all(writers);
	}

	awaitable<void> cmall_service::do_ws_write(size_t connection_id, client_connection_ptr connection_ptr)
	{
		auto& message_deque = connection_ptr->ws_client->message_channel;
		auto& ws			= connection_ptr->ws_client->ws_stream_;

		using namespace boost::asio::experimental::awaitable_operators;
		steady_timer t(co_await boost::asio::this_coro::executor);

		boost::asio::cancellation_state cs = co_await boost::asio::this_coro::cancellation_state;

		if (cs.slot().is_connected())
		{
			cs.slot().assign([&message_deque, &t](boost::asio::cancellation_type_t) mutable
			{
				boost::system::error_code ignore_ec;
				t.cancel(ignore_ec);
				message_deque.cancel();
			});
		}

		while (!m_abort)
			try
			{
				t.expires_from_now(std::chrono::seconds(15));
				std::variant<std::monostate, std::string> awaited_result
					= co_await (t.async_wait(use_awaitable)
						|| message_deque.async_receive(use_awaitable));
				if (awaited_result.index() == 0)
				{
					LOG_DBG << "coro: do_ws_write: [" << connection_id << "], send ping to client";
					co_await ws.async_ping("", use_awaitable); // timed out
				}
				else
				{
					auto message = std::get<1>(awaited_result);
					if (message.empty())
						co_return;
					co_await ws.async_write(boost::asio::buffer(message), use_awaitable);
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

	awaitable<void> cmall_service::close_all_ws()
	{
		co_await httpd::detail::map(m_ws_acceptors,
			[](auto&& a) mutable -> awaitable<void> { co_return co_await a.clean_shutdown(); });

		co_await httpd::detail::map(m_wss_acceptors,
			[](auto&& a) mutable -> awaitable<void> { co_return co_await a.clean_shutdown(); });

		co_await httpd::detail::map(m_ws_unix_acceptors,
			[](auto&& a) mutable -> awaitable<void> { co_return co_await a.clean_shutdown(); });

		LOG_DBG << "cmall_service::close_all_ws() success!";
	}

	promise<void(std::exception_ptr)> cmall_service::websocket_write(client_connection_ptr clientptr, std::string message)
	{
		// 本来 co_await co_spawn 就好，但是 gcc 上有 bug, 会崩
		// 所以返回 promise 然后在 promise 上 wait 就不会崩了.
		// 那为啥要在外面await 而不是这里 await 掉，是发现了新的情况.
		// 循环 for (c : all_client) promises.push_back(websocket_write(c, 'msg')); 的时候， 可以收集 promise
		// 然后 wait_all_promise(promises);
		if (clientptr->ws_client)
		{
		 	return boost::asio::co_spawn(
				clientptr->get_executor(),
				[clientptr, message = std::move(message)]() mutable -> awaitable<void>
				{
					clientptr->ws_client->message_channel.try_send(boost::system::error_code(), message);
					co_return;
				},
				use_promise);
		}
		throw std::runtime_error("write ws message on non-ws socket");
	}

	client_connection_ptr cmall_service::make_shared_connection(
		const boost::asio::any_io_executor& io, std::int64_t connection_id)
	{
		if constexpr (httpd::has_so_reuseport())
			return std::make_shared<client_connection>(io, connection_id);
		else
			return std::make_shared<client_connection>(m_io_context_pool.get_io_context(), connection_id);
	}

	client_connection_ptr cmall_service::make_shared_ssl_connection(const boost::asio::any_io_executor& io, std::int64_t connection_id)
	{
		if constexpr (httpd::has_so_reuseport())
			return std::make_shared<client_connection>(io, sslctx_, connection_id);
		else
			return std::make_shared<client_connection>(m_io_context_pool.get_io_context(), sslctx_, connection_id);
	}

	client_connection_ptr cmall_service::make_shared_unixsocket_connection(const boost::asio::any_io_executor&, std::int64_t connection_id)
	{
		return std::make_shared<client_connection>(m_io_context_pool.get_io_context(), connection_id, 0);
	}

	awaitable<void> cmall_service::client_connected(client_connection_ptr client_ptr)
	{
		using string_body = boost::beast::http::string_body;
		using fields	  = boost::beast::http::fields;
		using request	  = boost::beast::http::request<string_body>;
		using response	  = boost::beast::http::response<string_body>;

		const size_t connection_id = client_ptr->connection_id_;

		bool keep_alive = false;

		auto http_simple_error_page
			= [&client_ptr](auto body, auto status_code, unsigned version) mutable -> awaitable<void>
		{
			response res{ static_cast<boost::beast::http::status>(status_code), version };
			res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
			res.set(boost::beast::http::field::content_type, "text/html");
			res.keep_alive(false);
			res.body() = body;
			res.prepare_payload();

			boost::beast::http::serializer<false, string_body, fields> sr{ res };
			co_await boost::beast::http::async_write(client_ptr->tcp_stream, sr, use_awaitable);
			client_ptr->tcp_stream.close();
		};

		LOG_DBG << "coro created: handle_accepted_client( " << connection_id << ")";

		do
		{
			boost::beast::flat_buffer buffer;
			boost::beast::http::request_parser<boost::beast::http::string_body> parser_;

			parser_.body_limit(2000);

			co_await boost::beast::http::async_read(client_ptr->tcp_stream, buffer, parser_, use_awaitable);
			request req = parser_.release();
			keep_alive = req.keep_alive();

			// 这里是为了能提取到客户端的 IP 地址，即便服务本身运行在 nginx 的后面。
			auto x_real_ip = req["x-real-ip"];
			auto x_forwarded_for = req["x-forwarded-for"];
			if (x_real_ip.empty())
			{
				client_ptr->x_real_ip = client_ptr->remote_host_;
			}
			else
			{
				client_ptr->x_real_ip = x_real_ip;
			}

			if (!x_forwarded_for.empty())
			{
				std::vector<std::string> x_forwarded_clients;
				boost::split(x_forwarded_clients, x_forwarded_for, boost::is_any_of(", "));
				client_ptr->x_real_ip = x_forwarded_clients[0];
			}

			std::string_view target = req.target();

			LOG_DBG << "coro: handle_accepted_client: [" << connection_id << "], got request on " << target;

			if (target.empty() || target[0] != '/' || target.find("..") != boost::beast::string_view::npos)
			{
				co_await http_simple_error_page(
					"Illegal request-target", boost::beast::http::status::bad_request, req.version());
			}
			// 处理 HTTP 请求.
			else if (boost::beast::websocket::is_upgrade(req))
			{
				LOG_DBG << "ws client incoming: " << connection_id << ", remote: " << client_ptr->x_real_ip;

				if (!target.starts_with("/api"))
				{
					co_await http_simple_error_page(
						"not allowed", boost::beast::http::status::forbidden, req.version());
					break;
				}

				client_ptr->ws_client.emplace(client_ptr->tcp_stream);

				client_ptr->ws_client->ws_stream_.set_option(boost::beast::websocket::stream_base::decorator(
					[](auto& res) { res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING); }));

				co_await client_ptr->ws_client->ws_stream_.async_accept(req, use_awaitable);

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

				co_await (
					// 启动读写协程.
					do_ws_read(connection_id, client_ptr) || do_ws_write(connection_id, client_ptr));
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

				else if (target.starts_with("/repos"))
				{
					boost::match_results<std::string_view::const_iterator> w;
					if (boost::regex_match(
							target.begin(), target.end(), w, boost::regex("/repos/([0-9]+)/((images|css)/.+)")))
					{
						std::string merchant = w[1].str();
						std::string remains	 = w[2].str();

						int status_code = co_await render_git_repo_files(
							connection_id, merchant, remains, client_ptr->tcp_stream, req);

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
				else if (target.starts_with("/goods"))
				{
					boost::match_results<std::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/goods/([^/]+)/([^/]+)")))
					{
						std::string merchant = w[1].str();
						std::string goods_id = httpd::decodeURIComponent(w[2].str());

						int status_code = co_await render_goods_detail_content(
							connection_id, merchant, goods_id, client_ptr->tcp_stream, req.version(), req.keep_alive());

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
				// 这个 /scriptcallback/${merchant}/? 的调用, 都传给 scripts/callback.js.
				else if (target.starts_with("/scriptcallback"))
				{
					boost::match_results<std::string_view::const_iterator> w;
					if (boost::regex_match(target.begin(), target.end(), w, boost::regex("/scriptcallback/([0-9]+)(/.+)")))
					{
						std::string merchant = w[1].str();
						std::string remains = w[2].str();

						auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);
						boost::system::error_code ec;
						auto merchant_repo_ptr = get_merchant_git_repo(merchant_id, ec);

						if (ec)
						{
							co_await http_simple_error_page("ERRORED", 404, req.version());
							continue;
						}

						std::string callback_js = co_await merchant_repo_ptr->get_file_content("scripts/callback.js", ec);
						if (ec)
						{
							co_await http_simple_error_page("ERRORED", 404, req.version());
							continue;
						}

						std::map<std::string, std::string> script_env;

						for (auto& kv : req)
						{
							script_env.insert({std::string(kv.name_string()), std::string(kv.value())});
						}
						script_env.insert({"_METHOD", std::string(req.method_string())});
						script_env.insert({"_PATH", remains});

						// TODO 然后运行 callback.js
						std::string response_body = co_await script_runner.run_script(callback_js,
							req.body(), script_env, {});

						ec = co_await httpd::send_string_response_body(client_ptr->tcp_stream,
							response_body,
							httpd::make_http_last_modified(std::time(0) + 60),
							"text/plain",
							req.version(),
							keep_alive);
						if (ec)
							throw boost::system::system_error(ec);
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

				keep_alive &= !req.need_eof();
			}
		} while (keep_alive && (!m_abort));

		if (m_abort)
			co_return;

		if (!keep_alive)
		{
			co_await client_ptr->tcp_stream.async_teardown(boost::beast::role_type::server, use_awaitable);
		}

		LOG_DBG << "handle_accepted_client: HTTP connection closed : " << connection_id;
	}

	awaitable<void> cmall_service::client_disconnected(client_connection_ptr c)
	{
		std::unique_lock<std::shared_mutex> l(active_users_mtx);
		active_users.get<1>().erase(c->connection_id_);
		co_return;
	}
}
