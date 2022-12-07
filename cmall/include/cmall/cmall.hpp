//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "boost/asio/thread_pool.hpp"
#include "cmall/internal.hpp"
#include "cmall/database.hpp"

#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <vector>

#include "cmall/error_code.hpp"
#include "cmall/misc.hpp"
#include "persist_map.hpp"
#include "services/verifycode.hpp"
#include "services/persist_session.hpp"
#include "services/payment_service.hpp"
#include "services/merchant_git_repo.hpp"
#include "services/userscript_service.hpp"
#include "services/gitea_service.hpp"
#include "services/search_service.hpp"

#include "httpd/acceptor.hpp"
#include "httpd/ssl_acceptor.hpp"
#include "httpd/unix_acceptor.hpp"

#include "httpd/http_stream.hpp"

#include "cmall/client_connection.hpp"

using boost::asio::awaitable;
using boost::asio::experimental::promise;

namespace cmall {

	struct server_config
	{
		std::vector<std::string> ws_listens_;
		std::vector<std::string> wss_listens_;
		std::vector<std::string> ws_unix_listens_;

		cmall::db_config dbcfg_;

		std::filesystem::path session_cache_file;
		std::filesystem::path repo_root; // repos dir for gitea

		std::string gitea_api;
		std::string gitea_admin_token;
		std::string gitea_template_user;
		std::string gitea_template_reponame;

		std::string site_name;

		std::string netease_secret_id;
		std::string netease_secret_key;
		std::string netease_business_id;

		std::string tencent_secret_id;
		std::string tencent_secret_key;
	};


	inline int64_t client_connection_get_id(client_connection_ptr p)
	{
		return p->connection_id_;
	}

	inline uint64_t client_connection_get_user_id(client_connection_ptr p)
	{
		return p->session_info->user_info->uid_;
	}

	namespace tag{
		struct connection_id_tag{};
		struct userid_tag{};
		struct merchant_ranke_tag{};
		struct merchant_uid_tag{};
	}

	// 这个多索引map 用来快速找到同一个用户的 session
	typedef boost::multi_index_container<
		client_connection_ptr,
		boost::multi_index::indexed_by<
			boost::multi_index::sequenced<>,
			boost::multi_index::hashed_unique<
				boost::multi_index::tag<tag::connection_id_tag>,
				boost::multi_index::global_fun<client_connection_ptr, int64_t, &client_connection_get_id>
			>,
			boost::multi_index::hashed_non_unique<
				boost::multi_index::tag<tag::userid_tag>,
				boost::multi_index::global_fun<client_connection_ptr, uint64_t, &client_connection_get_user_id>
			>
		>
	> active_session_map;

	inline std::int64_t merchant_get_rank(std::shared_ptr<services::merchant_git_repo>)
	{
		return 0;
	}

	inline std::uint64_t merchant_get_uid(std::shared_ptr<services::merchant_git_repo> p)
	{
		return p->get_merchant_uid();
	}

	// 这个多索引map 用来快速找到同一个用户的 session
	typedef boost::multi_index_container<
		std::shared_ptr<services::merchant_git_repo>,
		boost::multi_index::indexed_by<
			boost::multi_index::sequenced<>,
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<tag::merchant_ranke_tag>,
				boost::multi_index::global_fun<std::shared_ptr<services::merchant_git_repo>, std::int64_t, &merchant_get_rank>
			>,
			boost::multi_index::hashed_unique<
				boost::multi_index::tag<tag::merchant_uid_tag>,
				boost::multi_index::global_fun<std::shared_ptr<services::merchant_git_repo>, std::uint64_t, &merchant_get_uid>
			>
		>
	> loaded_merchant_map;

	enum class req_method {
		recover_session,

		ping,

		user_fastlogin,
		user_prelogin,
		user_login,
		user_logout,
		user_islogin,
		user_info,
		user_apply_merchant,
		user_apply_info, // 审核状态.

		user_add_face,
		user_search_by_face, // demo

		user_list_recipient_address,
		user_add_recipient_address,
		user_modify_receipt_address,
		user_erase_receipt_address,
		user_3rd_kv_put,
		user_3rd_kv_get,
		user_3rd_kv_put_pubkey,
		user_3rd_kv_get_pubkey,

		cart_add,
		cart_mod,
		cart_del,
		cart_list,

		fav_add,
		fav_del,
		fav_list,

		order_create_cart,
		order_create_direct,
		order_detail,
		order_list,
		order_close,

		order_get_paymethods, // 获取支持的支付方式.
		order_get_pay_url,
		order_check_payment,

		search_goods,
		goods_list,
		goods_detail,
		goods_markdown,
		goods_merchant_index,

		merchant_info,
		merchant_get_sold_order_detail,
		merchant_sold_orders_check_payment,
		merchant_sold_orders_mark_payed,
		merchant_goods_list,
		merchant_keywords_list,
		merchant_list_sold_orders,
		merchant_sold_orders_add_kuaidi,
		merchant_delete_sold_orders,
		merchant_get_gitea_password,
		merchant_reset_gitea_password,
		merchant_create_apptoken,
		merchant_list_apptoken,
		merchant_delete_apptoken,
		merchant_alter_name,
		merchant_user_kv_get,

		admin_user_detail,
		admin_user_list,
		admin_user_ban,
		admin_list_applicants,
		admin_approve_merchant,
		admin_deny_applicant,
		admin_list_merchants,
		admin_disable_merchants,
		admin_reenable_merchants,
		admin_sudo,
		admin_sudo_cancel,
		admin_list_index_goods,
		admin_set_index_goods,
		admin_add_index_goods,
		admin_remove_index_goods,
	};

	class cmall_service
	{
		// c++11 noncopyable.
		cmall_service(cmall_service&&) = delete;
		cmall_service(const cmall_service&) = delete;
		cmall_service& operator=(const cmall_service&) = delete;

		using executor_type = boost::asio::any_io_executor;

	public:
		cmall_service(io_context_pool& ios, const server_config& config);
		~cmall_service();

	public:
		awaitable<bool> init_ws_acceptors();
		awaitable<bool> init_wss_acceptors(std::string_view cert, std::string_view key);
		awaitable<bool> init_ws_unix_acceptors();
		awaitable<bool> load_repos();
		awaitable<void> run_httpd();
		awaitable<void> stop();

	private:
		awaitable<bool> load_merchant_git(const cmall_merchant& merchant); // false if no git repo for merchant
		std::shared_ptr<services::merchant_git_repo> get_merchant_git_repo(std::uint64_t merchant_uid, boost::system::error_code& ec) const;
		std::shared_ptr<services::merchant_git_repo> get_merchant_git_repo(const cmall_merchant& merchant, boost::system::error_code& ec) const;
		std::shared_ptr<services::merchant_git_repo> get_merchant_git_repo(const cmall_merchant& merchant) const; // will throw if not found
		std::shared_ptr<services::merchant_git_repo> get_merchant_git_repo(std::uint64_t merchant_uid) const; // will throw if not found

		awaitable<void> repo_push_check(std::weak_ptr<services::merchant_git_repo>);

		// 这 3 个函数, 是 acceptor 需要的.
		client_connection_ptr        make_shared_connection(const boost::asio::any_io_executor& io, std::int64_t connection_id);
		client_connection_ptr        make_shared_ssl_connection(const boost::asio::any_io_executor& io, std::int64_t connection_id);
		client_connection_ptr        make_shared_unixsocket_connection(const boost::asio::any_io_executor& io, std::int64_t connection_id);

		awaitable<void> client_connected(client_connection_ptr);
		awaitable<void> client_disconnected(client_connection_ptr);

		awaitable<int> render_git_repo_files(size_t connection_id, std::string merchant, std::string path_in_repo, httpd::http_any_stream& client, boost::beast::http::request<boost::beast::http::string_body>);
		awaitable<int> render_goods_detail_content(std::string merchant, std::string goods_id, httpd::http_any_stream& client, const boost::beast::http::request<boost::beast::http::string_body>& req);
		awaitable<void> do_ws_read(size_t connection_id, client_connection_ptr);
		awaitable<void> do_ws_write(size_t connection_id, client_connection_ptr);

		awaitable<void> close_all_ws();

		promise<void(std::exception_ptr)> websocket_write(client_connection_ptr, std::string message);

		awaitable<void> alloca_sessionid(client_connection_ptr);
		awaitable<void> load_user_info(client_connection_ptr);

		awaitable<boost::json::object> handle_jsonrpc_call(client_connection_ptr, const std::string& method, boost::json::object params);

		awaitable<boost::json::object> handle_jsonrpc_user_api(client_connection_ptr, const req_method method, boost::json::object params);
		awaitable<boost::json::object> handle_jsonrpc_order_api(client_connection_ptr, const req_method method, boost::json::object params);
		awaitable<boost::json::object> handle_jsonrpc_cart_api(client_connection_ptr, const req_method method, boost::json::object params);
		awaitable<boost::json::object> handle_jsonrpc_fav_api(client_connection_ptr, const req_method method, boost::json::object params);
		awaitable<boost::json::object> handle_jsonrpc_goods_api(client_connection_ptr, const req_method method, boost::json::object params);

		awaitable<boost::json::object> handle_jsonrpc_merchant_api(client_connection_ptr, const req_method method, boost::json::object params);
		awaitable<boost::json::object> handle_jsonrpc_admin_api(client_connection_ptr, const req_method method, boost::json::object params);

		awaitable<std::vector<services::product>> list_index_goods(std::string baseurl);

		awaitable<void> send_notify_message(std::uint64_t uid_, const std::string&, std::int64_t exclude_connection);

		// round robing for acceptor.
		auto& get_executor(){ return m_io_context_pool.get_io_context(); }

		awaitable<bool> order_check_payment(cmall_order& order, const cmall_merchant& seller);
		awaitable<bool> order_mark_payed(cmall_order& order, const cmall_merchant& seller);

	private:
		io_context_pool& m_io_context_pool;
		boost::asio::io_context& m_io_context;

		server_config m_config;
		cmall_database m_database;

		boost::asio::thread_pool background_task_thread_pool;

		services::persist_session session_cache_map;
		services::verifycode telephone_verifier;
		services::payment payment_service;
		services::userscript script_runner;
		services::gitea gitea_service;
		services::search search_service;

		mutable std::shared_mutex merchant_repos_mtx;
		loaded_merchant_map merchant_repos;

		// ws 服务端相关.
		std::shared_mutex active_users_mtx;
		active_session_map active_users;

		std::once_flag check_admin_flag;

		boost::asio::ssl::context sslctx_;
		std::vector<httpd::acceptor<client_connection_ptr, cmall_service>> m_ws_acceptors;
		std::vector<httpd::ssl_acceptor<client_connection_ptr, cmall_service>> m_wss_acceptors;
		std::vector<httpd::unix_acceptor<client_connection_ptr, cmall_service>> m_ws_unix_acceptors;
		std::vector<promise<void(std::exception_ptr)>> m_background_threads;

		friend class httpd::acceptor<client_connection_ptr, cmall_service>;
		friend class httpd::ssl_acceptor<client_connection_ptr, cmall_service>;
		friend class httpd::unix_acceptor<client_connection_ptr, cmall_service>;
	};
}
