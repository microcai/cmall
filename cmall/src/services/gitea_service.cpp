
#include "stdafx.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <cstdlib>

#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost::asio::experimental::awaitable_operators;

#include "services/gitea_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"
#include "cmall/error_code.hpp"
#include "utils/uawaitable.hpp"

#include "utils/httpc.hpp"
#include "cmall/js_util.hpp"
#include "utils/uawaitable.hpp"

using boost::asio::use_awaitable;

namespace services
{
	static std::string gen_repo_user(std::uint64_t uid)
	{
		return std::format("m{}", uid);
	}

	struct gitea_impl
	{
		gitea_impl(const std::string& gitea_api, const std::string& token)
			: gitea_api(gitea_api)
			, admin_token(token)
		{}

		awaitable<bool> create_user(std::uint64_t uid, std::string password)
		{
			// 创建用户.
			auto username = gen_repo_user(uid);
			auto email = std::format("{}@{}.com", username, username);

			boost::json::value body = {
				{ "email", email },
				{ "username", username },
				{ "password", password },
				{ "must_change_password", false },
			};

			auto uri = std::format("{}/api/v1/admin/users", gitea_api);

			httpc::request_options_t opts {
				httpc::http::verb::post,
				uri,
				{},
				jsutil::json_to_string(body),
			};
			opts.headers.insert({"Content-Type", "application/json"});
			opts.headers.insert({"Authorization", std::format("token {}", admin_token)});

			boost::system::error_code ec;
			auto res = co_await httpc::request(std::move(opts));
			if (res.err.has_value())
				LOG_ERR << "create user error: " << res.err.value() << " uid: " << uid;

			// co_return res.code >= 200 && res.code < 300;
			co_return res.code == 201;
		}

		awaitable<bool> create_repo(std::uint64_t uid, std::string template_owner, std::string template_repo)
		{
			// 创建模板仓库.
			auto username = gen_repo_user(uid);

			boost::json::value body = {
				{ "owner", username },
				{ "name", "shop" },
				{ "private", true },
				{ "avatar", true },
				{ "description", "my shop" },
				{ "git_content", true },
				{ "git_hooks", true },
				{ "labels", true },
				{ "topics", true },
				{ "webhooks", true },
			};
			auto uri = std::format("{}/api/v1/repos/{}/{}/generate", gitea_api, template_owner, template_repo);
			httpc::request_options_t opts {
				httpc::http::verb::post,
				uri,
				{},
				jsutil::json_to_string(body),
			};
			opts.headers.insert({"Content-Type", "application/json"});
			opts.headers.insert({"Authorization", std::format("token {}", admin_token)});

			boost::system::error_code ec;
			auto res = co_await httpc::request(std::move(opts));
			if (res.err.has_value())
				LOG_ERR << "create user repo error: " << res.err.value() << " uid: " << uid;

			co_return res.code == 201;
		}

		awaitable<bool> init_user(std::uint64_t uid, std::string password, std::string template_owner = "admin", std::string template_repo = "shop-template")
		{
			bool ok = co_await create_user(uid, password);
			if (ok)
			{
				ok = co_await create_repo(uid, template_owner, template_repo);
			}
			co_return ok;
		}

		awaitable<bool> change_password(std::uint64_t uid, std::string password)
		{
			auto username = gen_repo_user(uid);

			boost::json::value body = {
				{ "login_name", username },
				{ "password", password },
			};

			auto uri = std::format("{}/api/v1/admin/users/{}", gitea_api, username);

			httpc::request_options_t opts {
				httpc::http::verb::patch,
				uri,
				{},
				jsutil::json_to_string(body),
			};
			opts.headers.insert({"Content-Type", "application/json"});
			opts.headers.insert({"Authorization", std::format("token {}", admin_token)});

			boost::system::error_code ec;
			auto res = co_await httpc::request(std::move(opts));
			if (res.err.has_value())
				LOG_ERR << "change user password error: " << res.err.value() << " uid: " << uid;

			// co_return res.code >= 200 && res.code < 300;
			co_return res.code == 200;
		}

		std::string gitea_api;
		std::string admin_token;
	};

	awaitable<bool> gitea::init_user(std::uint64_t uid, std::string password, std::string template_owner, std::string template_repo)
	{
		co_return co_await impl().init_user(uid, password, template_owner, template_repo);
	}

	awaitable<bool> gitea::change_password(std::uint64_t uid, std::string password)
	{
		co_return co_await impl().change_password(uid, password);
	}

	gitea::gitea(const std::string& gitea_api, const std::string& token)

	{
		static_assert(sizeof(obj_stor) >= sizeof(gitea_impl));
		std::construct_at(reinterpret_cast<gitea_impl*>(obj_stor.data()), gitea_api, token);
	}

	gitea::~gitea()
	{
		std::destroy_at(reinterpret_cast<gitea_impl*>(obj_stor.data()));
	}

	const gitea_impl& gitea::impl() const
	{
		return *reinterpret_cast<const gitea_impl*>(obj_stor.data());
	}

	gitea_impl& gitea::impl()
	{
		return *reinterpret_cast<gitea_impl*>(obj_stor.data());
	}

}
