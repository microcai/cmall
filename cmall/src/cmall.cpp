
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

#include "cmall/js_util.hpp"

#include "services/merchant_git_repo.hpp"

#include "cmall/conversion.hpp"

#include "httpd/http_misc_helper.hpp"
#include "httpd/header_helper.hpp"
#include "httpd/httpd.hpp"
#include "httpd/wait_all.hpp"
#include "dirmon/dirmon.hpp"

template <typename Iterator>
auto make_iterator_range(std::pair<Iterator, Iterator> pair)
{
	return boost::make_iterator_range(pair.first, pair.second);
}

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
		, gitea_service(config.gitea_api, config.gitea_admin_token)
		, sslctx_(boost::asio::ssl::context::tls_server)
	{
	}

	cmall_service::~cmall_service() { LOG_DBG << "~cmall_service()"; }

	awaitable<void> cmall_service::stop()
	{
		m_background_threads.clear();

		LOG_DBG << "close all ws...";
		co_await close_all_ws();

		LOG_DBG << "database shutdown...";
		m_database.shutdown();

		LOG_DBG << "cmall_service.stop()";
	}

	// false if no git repo for merchant
	awaitable<bool> cmall_service::load_merchant_git(const cmall_merchant& merchant)
	{
		if (services::merchant_git_repo::is_git_repo(merchant.repo_path))
		{
			{
				auto repo = std::make_shared<services::merchant_git_repo>(
					background_task_thread_pool, merchant.uid_, merchant.repo_path);
				std::unique_lock<std::shared_mutex> l(merchant_repos_mtx);
				merchant_repos.get<tag::merchant_uid_tag>().erase(merchant.uid_);
				auto insert_result = merchant_repos.get<tag::merchant_uid_tag>().insert(repo);
				BOOST_ASSERT_MSG(insert_result.second, "insert should always success!");
			}

			m_background_threads.push_back(
			boost::asio::co_spawn(background_task_thread_pool, [this, merchant_id = merchant.uid_]() mutable -> awaitable<void>
			{
				co_await this_coro::coro_yield();

				std::unique_lock<std::shared_mutex> l(merchant_repos_mtx);

				auto it = merchant_repos.get<tag::merchant_uid_tag>().find(merchant_id);

				if (it != merchant_repos.get<tag::merchant_uid_tag>().end())
				{
					std::shared_ptr<services::merchant_git_repo> repo = *it;
					l.unlock();
					co_await search_service.add_merchant(repo);
					co_await repo_push_check(std::move(repo));
				}
			}, use_promise));

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

	std::shared_ptr<services::merchant_git_repo> cmall_service::get_merchant_git_repo(std::uint64_t merchant_uid, boost::system::error_code& ec) const
	{
		std::shared_lock<std::shared_mutex> l(merchant_repos_mtx);
		auto& index_by_uid = merchant_repos.get<tag::merchant_uid_tag>();
		auto it = index_by_uid.find(merchant_uid);
		if (it == index_by_uid.end())
		{
			ec = cmall::error::merchcant_git_error;
			return {};
		}
		return * it;
	}

	std::shared_ptr<services::merchant_git_repo> cmall_service::get_merchant_git_repo(const cmall_merchant& merchant, boost::system::error_code& ec) const
	{
		return get_merchant_git_repo(merchant.uid_, ec);
	}

	 // will throw if not found
	std::shared_ptr<services::merchant_git_repo> cmall_service::get_merchant_git_repo(const cmall_merchant& merchant) const
	{
		return get_merchant_git_repo(merchant.uid_);
	}

	 // will throw if not found
	std::shared_ptr<services::merchant_git_repo> cmall_service::get_merchant_git_repo(std::uint64_t merchant_uid) const
	{
		std::shared_lock<std::shared_mutex> l(merchant_repos_mtx);
		auto& index_by_uid = merchant_repos.get<tag::merchant_uid_tag>();
		auto it = index_by_uid.find(merchant_uid);
		if (it == index_by_uid.end())
		{
			throw boost::system::system_error(cmall::error::merchcant_gitrepo_notfound);
		}
		return * it;
	}

	awaitable<void> cmall_service::repo_push_check(std::weak_ptr<services::merchant_git_repo> repo_)
	{
		std::string repo_dir;

		{
			auto repo = repo_.lock();
			if (!repo)
				co_return;
			repo_dir = repo->repo_path().string();
		}

		// dirmon::dirmon git_monitor(co_await boost::asio::this_coro::executor, repo_dir);

		for (;;)
		{
			using namespace boost::asio::experimental::awaitable_operators;

			awaitable_timer timer(co_await boost::asio::this_coro::executor);
			timer.expires_from_now(30s);

			co_await timer.async_wait();

			// auto awaited_result = co_await (
			// 	timer.async_wait()
			// 		||
			// 	git_monitor.async_wait_dirchange()
			// );

			// for (;;)
			// {
			// 	awaitable_timer timer(co_await boost::asio::this_coro::executor);
			// 	timer.expires_from_now(2s);

			// 	auto awaited_result = co_await (
			// 		timer.async_wait()
			// 			||
			// 		git_monitor.async_wait_dirchange()
			// 	);

			// 	// 只有持续 2s 左右的时间 git 仓库没有产生 inotify 消息，才认为 git 完成了动作.
			// 	// 不然就继续读取 inotify 获取消息.

			// 	if (awaited_result.index() == 0)
			// 	{
			// 		break;
			// 	}
			// }

			auto repo = repo_.lock();
			if (!repo)
				co_return;
			{
				if (co_await repo->check_repo_changed(co_await search_service.cached_head(repo) ))
				{
					LOG_DBG << std::format("repo {} git HEAD changed!", repo->repo_path().string());
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
			if (ec == cmall::error::object_too_large)
				co_return 413; // 413 Payload Too Large
			co_return 404;
		}

		// check for Range Header

		auto req_range = httpd::parse_range(req[boost::beast::http::field::range]);

		std::map<boost::beast::http::field, std::string> headers;

		headers.insert({boost::beast::http::field::accept_ranges, "bytes"});
		headers.insert({boost::beast::http::field::expires, httpd::make_http_last_modified(std::time(0) + 60)});
		headers.insert({boost::beast::http::field::content_type, httpd::get_mime_type_from_extension(std::filesystem::path(path_in_repo).extension().string())});


		if (req_range && ((res_body.length() != req_range->end) && (req_range->begin != 0)))
		{
			headers.insert({boost::beast::http::field::content_range, httpd::make_cpntent_range(req_range.value(), res_body.length())});


			// 执行 206 响应.
			ec = co_await httpd::send_string_response_body(client,
				req_range->end == 0 ? res_body.substr(req_range->begin): res_body.substr(req_range->begin, req_range->end - req_range->begin),
				std::move(headers),
				req.version(),
				req.keep_alive(),
				boost::beast::http::status::partial_content);
			if (ec)
				throw boost::system::system_error(ec);
			co_return 206;
		}
		else
		{
			ec = co_await httpd::send_string_response_body(client,
				std::move(res_body),
				std::move(headers),
				req.version(),
				req.keep_alive());
			if (ec)
				throw boost::system::system_error(ec);
			co_return 200;
		}
	}

	// 成功给用户返回内容, 返回 200. 如果没找到商品, 不要向 client 写任何数据, 直接放回 404, 由调用方统一返回错误页面.
	awaitable<int> cmall_service::render_goods_detail_content(std::string merchant, std::string goods_id, httpd::http_any_stream& client, const boost::beast::http::request<boost::beast::http::string_body>& req)
	{

		auto merchant_id = strtoll(merchant.c_str(), nullptr, 10);
		boost::system::error_code ec;
		auto merchant_repo_ptr = get_merchant_git_repo(merchant_id, ec);

		if (ec)
			co_return 404;

		std::string baseurl;

		auto user_agent = req[boost::beast::http::field::user_agent];
		if (!user_agent.empty())
		{
			// electron 客户端, 则 images 的地址, 要 replace 为 https://[host]
			if (user_agent.find("Electron") != std::string::npos)
			{
				baseurl = "https://";
				baseurl += req[boost::beast::http::field::host];
			}
		}

		bool return_md_type = true;

// 		for (auto& accept : make_iterator_range(req.equal_range(boost::beast::http::field::accept)))
// 		{
// 			return_md_type = true; //|= std::string::npos != accept.value().find("text/markdown");
// 		}

		std::map<boost::beast::http::field, std::string> headers;
		headers.insert({boost::beast::http::field::expires, httpd::make_http_last_modified(std::time(0) + 60)});

		std::string return_body;

		if (return_md_type)
		{
			return_body = co_await merchant_repo_ptr->get_product_detail(goods_id, baseurl);
			headers.insert({boost::beast::http::field::content_type, "text/markdown; charset=utf-8"});
		}
		else
		{
			std::string html_body = co_await merchant_repo_ptr->get_product_html(goods_id, baseurl);
			headers.insert({boost::beast::http::field::content_type, "text/html; charset=utf-8"});

			return_body = std::format(R"xhtml(<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <title>{}</title>
	<style>p {{display: grid}} </style>
  </head>
  <body>
      {}
  </body>
</html>
)xhtml", "查看商品详情", html_body);

			// return_body
		}

		ec = co_await httpd::send_string_response_body(client,
			return_body,
			headers,
			req.version(),
			req.keep_alive());
		if (ec)
			throw boost::system::system_error(ec);
		co_return 200;
	}

	awaitable<void> cmall_service::alloca_sessionid(client_connection_ptr connection_ptr)
	{
		client_connection& this_client = *connection_ptr;
		this_client.session_info = std::make_shared<services::client_session>();

		this_client.session_info->session_id = gen_uuid();
		co_await session_cache_map.save(*this_client.session_info);
	}

	awaitable<void> cmall_service::load_user_info(client_connection_ptr connection_ptr)
	{
		client_connection& this_client = *connection_ptr;

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

			if (this_client.session_info->sudo_mode)
			{
				administrators original_user;
				co_await m_database.async_load<administrators>(this_client.session_info->original_user->uid_, original_user);
				this_client.session_info->original_user = original_user;
			}

			std::unique_lock<std::shared_mutex> l(active_users_mtx);
			active_users.push_back(connection_ptr);
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
				std::string baseurl = jsutil::json_accessor(params).get_string("baseurl");

				this_client.ws_client->baseurl_ = baseurl;

				if (api_token.empty())
				{
					// 兼容老客户端, 如果基于 cookie 恢复, 就没有 sessionid, 但是已经有 connection_ptr->session_info
					if (!connection_ptr->session_info)
					{
						if (sessionid.empty())
						{
							if (!connection_ptr->session_info)
							{
								// 表示是第一次使用，所以创建一个新　session 给客户端.
								co_await alloca_sessionid(connection_ptr);
							}
						}
						else if (!(co_await session_cache_map.exist(sessionid)))
						{
							// 表示session 过期，所以创建一个新　session 给客户端.
							co_await alloca_sessionid(connection_ptr);
						}
						else
						{
							this_client.session_info
								= std::make_shared<services::client_session>(co_await session_cache_map.load(sessionid));
						}
					}

					if (this_client.session_info->user_info)
					{
						co_await load_user_info(connection_ptr);
					}

					std::string cookie_line = std::format("Session={}", this_client.session_info->session_id);

					reply_message["result"] = {
						{ "session_id", this_client.session_info->session_id },
						{ "isAdmin", static_cast<bool>(this_client.session_info->isAdmin) },
						{ "isMerchant", static_cast<bool>(this_client.session_info->isMerchant) },
						{ "isLogin", static_cast<bool>(this_client.session_info->user_info) },
						{ "isSudo", this_client.session_info->sudo_mode },
						{ "site_name", m_config.site_name },
						{ "cookie", cookie_line },
					};
				}
				else
				{
					cmall_apptoken appid_info;
					cmall_user db_user;

					if (co_await in_temp_api_token(api_token))
					{
						boost::smatch token_matched;

						if (!boost::regex_match(api_token, token_matched, boost::regex("([0-9]+)-(.+)")))
						{
							throw boost::system::system_error(error::internal_server_error);
						}

						std::string token_uid_part = token_matched[1].str();
						appid_info.uid_ = std::stoull(token_uid_part);

					}
					else if (co_await m_database.async_load<cmall_apptoken>(odb::query<cmall_apptoken>::apptoken == api_token, appid_info))
					{
						// load appid_info success!
					}
					else
					{
						throw boost::system::system_error(error::apitoken_invalid);
					}

					if (co_await m_database.async_load<cmall_user>(appid_info.uid_, db_user))
					{
						this_client.session_info = std::make_shared<services::client_session>();
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

					std::string cookie_line = std::format("Session={}", this_client.session_info->session_id);

					reply_message["result"] = {
						{ "login", "success" },
						{ "isMerchant", this_client.session_info->isMerchant },
						{ "isAdmin", this_client.session_info->isAdmin },
						{ "isSudo", this_client.session_info->sudo_mode },
						{ "site_name", m_config.site_name },
						{ "cookie", cookie_line },
					};

				}
			}
			break;
			case req_method::ping:
			{
				reply_message["result"] = "pong";
			}break;
			case req_method::user_fastlogin:
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
			case req_method::user_3rd_kv_put:
			case req_method::user_3rd_kv_get:
			case req_method::user_3rd_kv_put_pubkey:
			case req_method::user_3rd_kv_get_pubkey:
			case req_method::user_add_face:
				co_await ensure_login();
			case req_method::user_search_by_face:
				co_return co_await handle_jsonrpc_user_api(connection_ptr, method.value(), params);
				break;

			case req_method::cart_add:
			case req_method::cart_mod:
			case req_method::cart_del:
			case req_method::cart_list:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_cart_api(connection_ptr, method.value(), params);
				break;
			case req_method::fav_add:
			case req_method::fav_del:
			case req_method::fav_list:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_fav_api(connection_ptr, method.value(), params);
				break;

			case req_method::order_create_cart:
			case req_method::order_create_direct:
			case req_method::order_detail:
			case req_method::order_close:
			case req_method::order_list:
			case req_method::order_get_paymethods:
			case req_method::order_get_pay_url:
			case req_method::order_check_payment:
				co_await ensure_login();
				co_return co_await handle_jsonrpc_order_api(connection_ptr, method.value(), params);
				break;
			case req_method::search_goods:
			case req_method::goods_list:
			case req_method::goods_detail:
			case req_method::goods_markdown:
			case req_method::goods_merchant_index:
				co_return co_await handle_jsonrpc_goods_api(connection_ptr, method.value(), params);
				break;
			case req_method::merchant_info:
			case req_method::merchant_get_sold_order_detail:
			case req_method::merchant_sold_orders_check_payment:
			case req_method::merchant_sold_orders_mark_payed:
			case req_method::merchant_goods_list:
			case req_method::merchant_keywords_list:
			case req_method::merchant_list_sold_orders:
			case req_method::merchant_sold_orders_add_kuaidi:
			case req_method::merchant_delete_sold_orders:
			case req_method::merchant_get_gitea_password:
			case req_method::merchant_reset_gitea_password:
			case req_method::merchant_create_apptoken:
			case req_method::merchant_list_apptoken:
			case req_method::merchant_delete_apptoken:
			case req_method::merchant_alter_name:
			case req_method::merchant_user_kv_get:
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
			case req_method::admin_sudo:
				co_await ensure_login(true);
				co_return co_await handle_jsonrpc_admin_api(connection_ptr, method.value(), params);
			case req_method::admin_sudo_cancel:
				// check original user
				if (this_client.session_info->sudo_mode && this_client.session_info->original_user)
				{
					co_return co_await handle_jsonrpc_admin_api(connection_ptr, method.value(), params);
				}
				else
				{
					throw boost::system::system_error(error::not_in_sudo_mode);
				}
				break;
			case req_method::admin_list_index_goods:
			case req_method::admin_set_index_goods:
			case req_method::admin_add_index_goods:
			case req_method::admin_remove_index_goods:
				co_await ensure_login(true);
				co_return co_await handle_jsonrpc_admin_api(connection_ptr, method.value(), params);
				break;
			default:
				throw boost::system::system_error(error::not_implemented);
		}

		co_return reply_message;
	}

	awaitable<bool> cmall_service::in_temp_api_token(std::string api_token)
	{
		std::shared_lock<std::shared_mutex> l (temp_api_token_mtx);
		co_return temp_api_token.contains(api_token);
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
}
