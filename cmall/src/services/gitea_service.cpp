
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
#include <unistd.h>

using namespace boost::asio::experimental::awaitable_operators;

#include "services/gitea_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"
#include "cmall/error_code.hpp"
#include "utils/uawaitable.hpp"

#include "../sandbox.hpp"

#ifdef __linux__

#include <sys/types.h>
#include <pwd.h>

static bool change_user(const char* name)
{
	struct passwd pwd;
	struct passwd *result;
	char buf[16384];

	int ok = getpwnam_r(name, &pwd, buf, 16384, &result);
	if (ok == 0)
	{
		setgid(pwd.pw_gid);
		setuid(pwd.pw_uid);
	}

	return ok == 0;
}

#endif

namespace services
{
	struct gitea_impl
	{
		gitea_impl(boost::asio::io_context& io)
			: io(io)
		{}

		std::string gen_repo_user(std::uint64_t uid)
		{
			return std::format("m{}", uid);
		}

		boost::asio::awaitable<bool> call_gitea_cli(const std::vector<std::string>& gitea_args)
		{
			using namespace boost::process;

			async_pipe out(io);
			child cp(io, search_path("gitea"), args(gitea_args), std_out > out
			#ifdef __linux
				, extend::on_exec_setup=[](auto& exec){
					change_user("gitea");
				}
			#endif
			);

			std::string output;
			boost::system::error_code ec;
			co_await boost::asio::async_read_until(out, boost::asio::dynamic_buffer(output), '\n', asio_util::use_awaitable[ec]);

			cp.wait_for(std::chrono::milliseconds(120), ec);
			if (cp.running())
				cp.terminate();

			co_return cp.exit_code() == EXIT_SUCCESS;
		}

		boost::asio::awaitable<bool> create_user(std::uint64_t uid, const std::string& password)
		{
			// 创建用户.
			auto username = gen_repo_user(uid);
			auto email = std::format("{}@{}.com", username, username);

			std::vector<std::string> gitea_args = {
				"admin",
				"user",
				"create",
				"--username",
				username,
				"--password",
				password,
				"--email",
				email
			};

			return call_gitea_cli(gitea_args);
		}

		boost::asio::awaitable<bool> create_repo(std::uint64_t uid, const std::string& template_dir)
		{
			// 创建模板仓库.
			auto username = gen_repo_user(uid);

			std::vector<std::string> gitea_args = {
				"admin",
				"restore-repo",
				"--repo_dir",
				template_dir,
				"--owner_name",
				username,
				"--repo_name",
				"goods1"
			};

			return call_gitea_cli(gitea_args);
		}

		boost::asio::awaitable<bool> init_user(std::uint64_t uid, const std::string& password, const std::string& template_dir)
		{
			bool ok = co_await create_user(uid, password);
			if (ok)
			{
				ok = co_await create_repo(uid, template_dir);
			}
			co_return ok;
		}

		boost::asio::awaitable<bool> change_password(std::uint64_t uid, const std::string& password)
		{
			auto username = gen_repo_user(uid);

			std::vector<std::string> gitea_args = {
				"admin",
				"user",
				"change-password",
				"--username",
				username,
				"--password",
				password
			};
			return call_gitea_cli(gitea_args);
		}

		boost::asio::io_context& io;
	};

	boost::asio::awaitable<bool> gitea::init_user(std::uint64_t uid, const std::string& password, const std::string& template_dir)
	{
		return impl().init_user(uid, password, template_dir);
	}
	boost::asio::awaitable<bool> gitea::change_password(std::uint64_t uid, const std::string& password)
	{
		return impl().change_password(uid, password);
	}

	gitea::gitea(boost::asio::io_context& io)
	{
		static_assert(sizeof(obj_stor) >= sizeof(gitea_impl));
		std::construct_at(reinterpret_cast<gitea_impl*>(obj_stor.data()), io);
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
