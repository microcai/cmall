
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

#ifdef BOOST_POSIX_API

#include <sys/types.h>
#include <pwd.h>
#include <sys/syscall.h>
#include <unistd.h>

struct unixuserinfo
{
	std::string username;
	std::string homedir;
	std::string shell;

	int uid, gid;
};

static unixuserinfo get_unix_user_info(std::string_view name)
{
	unixuserinfo ret;
	struct passwd pwd_storeage;
	struct passwd *result;
	std::vector<char> buf(sysconf(_SC_GETPW_R_SIZE_MAX) == -1 ? 16384 : sysconf(_SC_GETPW_R_SIZE_MAX));

	int ok = getpwnam_r(name.data(), &pwd_storeage, buf.data(), buf.capacity(), &result);
	if (ok == 0)
	{
		ret.homedir = result->pw_dir;
		ret.username = std::string(name);
		ret.shell = result->pw_shell;
		ret.uid = result->pw_uid;
		ret.gid = result->pw_gid;
		return ret;
	}

	throw boost::system::system_error(boost::asio::error::not_found);
}

static void change_user(int uid, int gid)
{
	setgid(gid);
	setuid(uid);
}


static boost::asio::awaitable<int> async_wait_child(boost::process::child& child)
{
	auto executor = co_await boost::asio::this_coro::executor;
	int pidfd_ = syscall(SYS_pidfd_open, child.id(), 0);

	boost::asio::posix::stream_descriptor pidfd(executor, pidfd_);

	co_await pidfd.async_wait(boost::asio::posix::stream_descriptor::wait_read, boost::asio::use_awaitable);

	int status = 0;
	pid_t ret = waitpid(child.id(), &status, 0);
	co_return status;
}

#endif

#ifdef _WIN32

#include <boost/asio/windows/object_handle.hpp>

static boost::asio::awaitable<int> async_wait_child(boost::process::child& child)
{
	auto executor = co_await boost::asio::this_coro::executor;
	boost::asio::windows::object_handle pidfd(executor, child.native_handle());

	co_await pidfd.async_wait(boost::asio::use_awaitable);

	co_return child.wait_for(std::chrono::milliseconds(2));
}

#endif

static std::string flaten_args(auto args)
{
	std::string cmdline;
	for (auto arg : args)
	{
		cmdline += " ";
		cmdline += arg;
	}
	return cmdline;
}

namespace services
{
	static std::string gen_repo_user(std::uint64_t uid)
	{
		return std::format("m{}", uid);
	}

	struct gitea_impl
	{
		gitea_impl(boost::asio::io_context& io)
			: io(io)
		{}

		boost::asio::awaitable<bool> call_gitea_cli(std::vector<std::string> gitea_args)
		{
			using namespace boost::process;

			LOG_DBG << "executing gitea " << flaten_args(gitea_args);
#ifdef BOOST_POSIX_API
			boost::process::environment gitea_process_env = boost::this_process::environment();
			auto gitea_userinfo = get_unix_user_info("gitea");

			gitea_process_env.set("HOME", gitea_userinfo.homedir);
			gitea_process_env.set("USER", "gitea");
			gitea_process_env.set("LANG", "C");
#endif
			child cp(io, search_path("gitea"), boost::process::args(gitea_args)
			#ifdef BOOST_POSIX_API
				, gitea_process_env, extend::on_exec_setup=[gitea_userinfo](auto& exec){
					::change_user(gitea_userinfo.uid, gitea_userinfo.gid);
				}
			#endif
			);

			int exit_code = co_await async_wait_child(cp);

			LOG_DBG << "executing gitea return " << exit_code;

			co_return exit_code == EXIT_SUCCESS;
		}

		boost::asio::awaitable<bool> create_user(std::uint64_t uid, std::string password)
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

			co_return co_await call_gitea_cli(gitea_args);
		}

		boost::asio::awaitable<bool> create_repo(std::uint64_t uid, std::string template_dir)
		{
			// 创建模板仓库.
			auto username = gen_repo_user(uid);

			std::vector<std::string> gitea_args = {
				"restore-repo",
				"--repo_dir",
				template_dir,
				"--owner_name",
				username,
				"--repo_name",
				"shop"
			};

			co_return co_await call_gitea_cli(gitea_args);
		}

		boost::asio::awaitable<bool> init_user(std::uint64_t uid, std::string password, std::string template_dir)
		{
			bool ok = co_await create_user(uid, password);
			if (ok)
			{
				ok = co_await create_repo(uid, template_dir);
			}
			co_return ok;
		}

		boost::asio::awaitable<bool> change_password(std::uint64_t uid, std::string password)
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
			co_return co_await call_gitea_cli(gitea_args);
		}

		boost::asio::io_context& io;
	};

	boost::asio::awaitable<bool> gitea::init_user(std::uint64_t uid, std::string password, std::string template_dir)
	{
		co_return co_await impl().init_user(uid, password, template_dir);
	}
	boost::asio::awaitable<bool> gitea::change_password(std::uint64_t uid, std::string password)
	{
		co_return co_await impl().change_password(uid, password);
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
