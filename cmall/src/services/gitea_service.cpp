
#include "stdafx.hpp"

#include "boost/asio/buffer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/read_until.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <boost/process/extend.hpp>
#include <boost/process/handles.hpp>
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
		gitea_impl(boost::asio::io_context& io, const std::string& gitea_api, const std::string& token)
			: io(io)
			, gitea_api(gitea_api)
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
		awaitable<std::string> create_user_token(std::uint64_t uid, std::string password)
		{
			// 创建用户.
			auto now = to_string(boost::posix_time::second_clock::local_time());
			auto username = gen_repo_user(uid);
			auto token_name = username + "_" + now;

			boost::json::value body = {
				{ "name", token_name },
			};

			auto uri = std::format("{}/api/v1/users/{}/tokens", gitea_api, username);

			httpc::request_options_t opts {
				httpc::http::verb::post,
				uri,
				{},
				jsutil::json_to_string(body),
			};
			opts.headers.insert({"Content-Type", "application/json"});
			auto auth_info = std::format("{}:{}", username, password);
			opts.headers.insert({"Authorization", std::format("Basic {}", base64_encode(auth_info))});

			boost::system::error_code ec;
			auto res = co_await httpc::request(std::move(opts));
			std::string token;
			do
			{
				if (res.err.has_value())
				{
					LOG_ERR << "create user error: " << res.err.value() << " uid: " << uid;
					break;
				}
				if (res.code != 201 || !res.content_type.starts_with("application/json"))
				{
					LOG_ERR << "create user error: " << res.body;
					break;
				}

				auto jv = boost::json::parse(res.body, ec, {}, { 64, false, false, false });
				if (ec || !jv.is_object())
				{
					LOG_ERR << "create user response error: " << res.body;
					break;
				}

				if (jv.as_object().contains("sha1") || !jv.at("sha1").is_string())
				{
					LOG_ERR << "create user response error: " << res.body << ", no sha1 field or format error";
					break;
				}

				token = jv.at("sha1").as_string();

			} while (false);

			co_return token;
		}

		awaitable<bool> create_repo(std::uint64_t uid, std::string user_token)
		{
			// 创建模板仓库.
			auto username = gen_repo_user(uid);

			// TODO: 目前写死.
			std::string template_owner = "admin";
			std::string template_repo = "shop-template";

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
			opts.headers.insert({"Authorization", std::format("token {}", user_token)});

			boost::system::error_code ec;
			auto res = co_await httpc::request(std::move(opts));
			if (res.err.has_value())
				LOG_ERR << "create user repo error: " << res.err.value() << " uid: " << uid;

			co_return res.code == 201;
		}

		awaitable<bool> init_user(std::uint64_t uid, std::string password, std::string template_dir)
		{
			bool ok = co_await create_user(uid, password);
			if (ok)
			{
				auto user_token = co_await create_user_token(uid, password);
				ok = co_await create_repo(uid, user_token);
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

		boost::asio::io_context& io;

		std::string gitea_api;
		std::string admin_token;
	};

	awaitable<bool> gitea::init_user(std::uint64_t uid, std::string password, std::string template_dir)
	{
		co_return co_await impl().init_user(uid, password, template_dir);
	}

	awaitable<bool> gitea::change_password(std::uint64_t uid, std::string password)
	{
		co_return co_await impl().change_password(uid, password);
	}

	gitea::gitea(boost::asio::io_context& io, const std::string& gitea_api, const std::string& token)

	{
		static_assert(sizeof(obj_stor) >= sizeof(gitea_impl));
		std::construct_at(reinterpret_cast<gitea_impl*>(obj_stor.data()), io, gitea_api, token);
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
