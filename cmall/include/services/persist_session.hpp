
#pragma once

#include <optional>
#include <memory>
#include <filesystem>
#include <chrono>
#include <set>
#include <boost/asio/awaitable.hpp>
#include "services/verifycode.hpp"
#include "cmall/database.hpp"

using boost::asio::awaitable;

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

	struct client_session
	{
		// 使用 session_id 可以快速找回已登录回话.
		std::string session_id;
		std::optional<cmall_user> user_info;
		std::optional<cmall_merchant> merchant_info;
		std::optional<administrators> admin_info;
		bool isAdmin = false;
		bool isMerchant = false;
		std::string verify_telephone;
		std::optional<verify_session> verify_session_cookie;
		bool sudo_mode = false;
		std::optional<administrators> original_user;

		void clear()
		{
			user_info.reset();
			merchant_info.reset();
			admin_info.reset();
			isAdmin = false;
			isMerchant = false;
			verify_telephone.clear();
			verify_session_cookie.reset();
		}
	};

	struct persist_session_impl;
	class persist_session
	{
	public:
		persist_session(cmall::cmall_database& db);
		~persist_session();

		awaitable<bool> exist(std::string session_id) const;
		awaitable<client_session> load(std::string session_id) const;
		awaitable<void> save(const client_session& session, std::chrono::duration<int> lifetime = std::chrono::seconds(86400 * 30));
		awaitable<void> save(std::string session_id, const client_session& session, std::chrono::duration<int> lifetime = std::chrono::seconds(86400 * 30));
		awaitable<void> update_lifetime(std::string session_id, std::chrono::duration<int> lifetime = std::chrono::seconds(86400 * 30));

	private:
		const persist_session_impl& impl() const;
		persist_session_impl& impl();

		std::array<char, 512> obj_stor;
	};

}
