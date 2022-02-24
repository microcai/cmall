
#pragma once

#include <memory>
#include <filesystem>
#include <chrono>
#include <boost/asio/awaitable.hpp>
#include "services/verifycode.hpp"
#include "cmall/db.hpp"

namespace services
{
	enum client_capabilities
	{
		browerable,                                               // 可浏览商品.
		orderable,                                                // 可下单.
		deletable,                                                // 可下架商品.
		editable,                                                 // 可编辑自己的商品.
		price_alterable,                                          // 可修改售价.
	};

	struct authorized_client_info
	{
		cmall_chives db_user_entry;
		std::set<client_capabilities> cap;
	};

	struct client_session
	{
		// 使用 session_id 可以快速找回已登录回话.
		std::string session_id;
		std::optional<authorized_client_info> user_info;
		std::optional<verify_session> verify_session_cookie;
	};

	struct persist_session_impl;
	class persist_session
	{
	public:
		persist_session(std::filesystem::path persist_file);
		~persist_session();

		boost::asio::awaitable<bool> exist(std::string_view session_id) const;
		boost::asio::awaitable<client_session> load(std::string_view session_id) const;
		boost::asio::awaitable<void> save(std::string_view session_id, const client_session& session, std::chrono::duration<int> lifetime = std::chrono::seconds(86400 * 30));
		boost::asio::awaitable<void> update_lifetime(std::string_view session_id, std::chrono::duration<int> lifetime = std::chrono::seconds(86400 * 30));

	private:
		const persist_session_impl& impl() const;
		persist_session_impl& impl();

		std::array<char, 512> obj_stor;
	};

}