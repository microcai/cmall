﻿
#include "io_context_pool.hpp"

#include <algorithm>
#include <functional>

#include <boost/asio/experimental/awaitable_operators.hpp>

#ifdef __linux__
#  include <sys/resource.h>
#  include <systemd/sd-daemon.h>

# ifndef HAVE_UNAME
#  define HAVE_UNAME
# endif

#elif _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>
#endif

#ifdef HAVE_UNAME
#  include <sys/utsname.h>
#endif

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "cmall/version.hpp"
#include "cmall/internal.hpp"
#include "cmall/cmall.hpp"
#include "utils/uawaitable.hpp"

static int platform_init()
{
#if defined(WIN32) || defined(_WIN32)
	/* Disable the "application crashed" popup. */
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
		SEM_NOOPENFILEERRORBOX);

#if defined(DEBUG) ||defined(_DEBUG)
	//	_CrtDumpMemoryLeaks();
	// 	int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	// 	flags |= _CRTDBG_LEAK_CHECK_DF;
	// 	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	// 	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	// 	_CrtSetDbgFlag(flags);
#endif

#if !defined(__MINGW32__)
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif

	_setmode(0, _O_BINARY);
	_setmode(1, _O_BINARY);
	_setmode(2, _O_BINARY);

	/* Disable stdio output buffering. */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	/* Enable minidump when application crashed. */
#elif defined(__linux__)
	rlimit of = { 50000, 100000 };
	if (setrlimit(RLIMIT_NOFILE, &of) < 0)
	{
		perror("setrlimit for nofile");
	}
	struct rlimit core_limit;
	core_limit.rlim_cur = RLIM_INFINITY;
	core_limit.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &core_limit) < 0)
	{
		perror("setrlimit for coredump");
	}

	/* Set the stack size programmatically with setrlimit */
	rlimit rl;
	int result = getrlimit(RLIMIT_STACK, &rl);
	if (result == 0)
	{
		const rlim_t stack_size = 100 * 1024 * 1024;
		if (rl.rlim_cur < stack_size)
		{
			rl.rlim_cur = stack_size;
			result = setrlimit(RLIMIT_STACK, &rl);
			if (result != 0)
				perror("setrlimit for stack size");
		}
	}
#endif

	std::ios::sync_with_stdio(false);

#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
#else
	set_thread_name("mainthread");
#endif
	return 0;
}

static std::string version_info()
{
	std::string os_name;

#ifdef _WIN32
#ifdef _WIN64
	os_name = "Windows (64bit)";
#else
	os_name = "Windows (32bit)";
#endif // _WIN64

#elif defined(HAVE_UNAME)
	utsname un;
	uname(&un);
	os_name = un.sysname;
	os_name += " ";
	os_name += un.release;

	// extract_linux_version from un.release;
	int ma_ver, mi_ver, patch_ver;
	sscanf(un.release, "%d.%d.%d", &ma_ver, &mi_ver, &patch_ver);

	int kernel_version = ma_ver * 10000 + mi_ver * 100 + patch_ver;

	if (std::string(un.sysname) == "Linux" && kernel_version < 50800)
	{
		std::cerr << "you are running a very very OLD kernel. please upgrade your system" << std::endl;
	}

#elif defined(__APPLE__)
	os_name = "Drawin";
#else
	os_name = "unknow";
#endif // _WIN32

	std::ostringstream oss;
	oss << "cmall version: v" << CMALL_VERSION << ", " << CMALL_GIT_REVISION
		<< " built on " << __DATE__ << " " << __TIME__ << " runs on " << os_name << ", " << BOOST_COMPILER;

	return oss.str();
}

static awaitable<std::string> get_default_cache_dir()
{
#ifndef WIN32
	if (::access("/var/lib/cmall", W_OK | R_OK | X_OK) ==  0)
		co_return "/var/lib/cmall";
#endif
	std::error_code ec;
	if (std::filesystem::create_directory("session_cache", ec))
	{
		co_return "session_cache";
	}

	if (std::filesystem::exists("session_cache"))
		co_return "session_cache";
	co_return ".";
}

awaitable<int> co_main(int argc, char** argv, io_context_pool& ios)
{
	std::vector<std::string> ws_listens, wss_listens, ws_unix_listens;
	std::string session_cache;

	std::string db_name;
	std::string db_host;
	std::string db_user = "postgres";
	std::string db_passwd;
	unsigned short db_port;

	std::string template_dir;
	std::string repo_root;

	std::string cert_file, key_file;

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Help message.")
		("version", "Current version.")

		("session_cache", po::value<std::string>(&session_cache)->default_value(co_await get_default_cache_dir()), "the dir for session cache")

		("ws", po::value<std::vector<std::string>>(&ws_listens)->multitoken(), "For websocket server listen.")
		("wss", po::value<std::vector<std::string>>(&wss_listens)->multitoken(), "For SSL websocket server listen.")
		("ws_unix", po::value<std::vector<std::string>>(&ws_unix_listens)->multitoken(), "For (unix socket) websocket server listen.")
		("db_name", po::value<std::string>(&db_name)->default_value("cmall"), "Database name.")
		("db_host", po::value<std::string>(&db_host)->default_value(""), "Database host.")
		("db_port", po::value<unsigned short>(&db_port)->default_value(5432), "Database port.")
		("db_user", po::value<std::string>(&db_user)->default_value("postgres"), "Database user.")
		("db_passwd", po::value<std::string>(&db_passwd)->default_value("postgres"), "Database password.")
		("template_dir", po::value<std::string>(&template_dir)->default_value("/var/lib/gitea/example/shop.git"), "gitea template dir")
		("repo_root", po::value<std::string>(&repo_root)->default_value("/repos"), "gitea repo base dir")
		("cert", po::value<std::string>(&cert_file), "ssl cert file")
		("key", po::value<std::string>(&key_file), "ssl private key file")
		;

	try
	{
		// 解析命令行.
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		// 输出版本信息.
		LOG_INFO << version_info();

		// 帮助输出.
		if (vm.count("help") || argc == 1)
		{
			std::cout << desc;
			co_return EXIT_SUCCESS;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "exception: " << e.what() << std::endl;
		co_return EXIT_FAILURE;
	}

	boost::asio::signal_set terminator_signal(ios.get_io_context());
	terminator_signal.add(SIGINT);
	terminator_signal.add(SIGTERM);

#if defined(SIGQUIT)
	terminator_signal.add(SIGQUIT);
#endif // defined(SIGQUIT)

	cmall::server_config cfg;
	cmall::db_config dbcfg;

	dbcfg.host_ = db_host;
	dbcfg.password_ = db_passwd;
	dbcfg.user_ = db_user;
	dbcfg.dbname_ = db_name;
	dbcfg.port_ = db_port;

	cfg.dbcfg_ = dbcfg;
	cfg.ws_listens_ = ws_listens;
	cfg.wss_listens_ = wss_listens;
	cfg.ws_unix_listens_ = ws_unix_listens;
	cfg.session_cache_file = session_cache;
	cfg.gitea_template_location = template_dir;
	cfg.repo_root = repo_root;

	cmall::cmall_service xsrv(ios, cfg);

	using namespace boost::asio::experimental::awaitable_operators;

	if (!co_await xsrv.load_repos())
		co_return EXIT_FAILURE;

	// 初始化ws acceptors.
	co_await xsrv.init_ws_acceptors();

	if (!wss_listens.empty())
	{
		boost::system::error_code ec;

		boost::asio::stream_file pem_cert(ios.get_io_context(), cert_file, boost::asio::file_base::read_only);
		boost::asio::stream_file privatekey(ios.get_io_context(), key_file, boost::asio::file_base::read_only);

		std::string cert_file_content;
		std::string key_file_content;
		auto cert_buf = boost::asio::dynamic_buffer(cert_file_content);
		auto privatekey_buf = boost::asio::dynamic_buffer(key_file_content);

		co_await (
			co_await boost::asio::async_read(pem_cert, cert_buf, boost::asio::transfer_all(), asio_util::use_awaitable[ec])
			&&
			co_await boost::asio::async_read(privatekey, privatekey_buf, boost::asio::transfer_all(), asio_util::use_awaitable[ec])
		);

		// 初始化ws acceptors.
		co_await xsrv.init_wss_acceptors(cert_file_content, key_file_content);
	}

	co_await xsrv.init_ws_unix_acceptors();

	// 处理中止信号.
	co_await(
		xsrv.run_httpd()
			||
		terminator_signal.async_wait(use_awaitable)
	);

	terminator_signal.clear();

	LOG_DBG << "terminator is called!";

	co_await xsrv.stop();
	LOG_DBG << "xsrv.stop() is called!";

	co_return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
	platform_init();

	int main_return;

	io_context_pool ios(boost::thread::hardware_concurrency());

	boost::asio::co_spawn(ios.server_io_context(), co_main(argc, argv, ios), [&](std::exception_ptr e, int ret){
		if (e)
			std::rethrow_exception(e);
		main_return = ret;
		ios.stop();
	});
#if 0
	pthread_atfork([](){
		ios.notify_fork(boost::asio::execution_context::fork_prepare);
	}, [](){
		ios.notify_fork(boost::asio::execution_context::fork_parent);
	}, [](){
		ios.notify_fork(boost::asio::execution_context::fork_child);
	});
#endif
	ios.run();
	return main_return;
}
