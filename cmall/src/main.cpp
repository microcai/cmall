
#include "stdafx.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "io_context_pool.hpp"

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

#ifdef __linux__
	// 把 EXE 自己所在的目录也放入 PATH 搜索路径.
	std::vector<std::string> searchpath;
	std::string path_env = getenv("PATH");
	std::string self_dir = std::filesystem::read_symlink("/proc/self/exe").parent_path().string();

	boost::algorithm::split(searchpath, path_env, boost::is_any_of(":"));

	if (std::find(searchpath.begin(), searchpath.end(), self_dir) == searchpath.end())
	{
		path_env = path_env + ":" + self_dir;
		setenv("PATH", path_env.c_str(), 1);
	}
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

#elif defined(__linux__)
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

	std::string repo_root;

	std::string gitea_token, gitea_api_path = "http://localhost:3000", gitea_template_user, gitea_template_reponame = "shop-template";
	std::string netease_secret_id, netease_secret_key, netease_business_id;
	std::string tencent_secret_id, tentcent_secret_key;

	std::string cert_file, key_file;

	std::string site_name;
	std::string config;

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Help message.")
		("version", "Current version.")

		("session_cache", po::value<std::string>(&session_cache)->default_value(co_await get_default_cache_dir())->value_name("dir"), "the dir for session cache")

		("ws", po::value<std::vector<std::string>>(&ws_listens)->multitoken()->value_name("ip:port [ip:port ...]"), "For websocket server listen.")
		("wss", po::value<std::vector<std::string>>(&wss_listens)->multitoken()->value_name("ip:port [ip:port ...]"), "For SSL websocket server listen.")
		("ws_unix", po::value<std::vector<std::string>>(&ws_unix_listens)->multitoken()->value_name("path [path ...]"), "For (unix socket) websocket server listen.")
		("db_name", po::value<std::string>(&db_name)->default_value("cmall")->value_name("db"), "Database name.")
		("db_host", po::value<std::string>(&db_host)->default_value("")->value_name("host"), "Database host.")
		("db_port", po::value<unsigned short>(&db_port)->default_value(5432)->value_name("port"), "Database port.")
		("db_user", po::value<std::string>(&db_user)->default_value("postgres")->value_name("user"), "Database user.")
		("db_passwd", po::value<std::string>(&db_passwd)->default_value("postgres")->value_name("passwd"), "Database password.")
		("repo_root", po::value<std::string>(&repo_root)->default_value("/repos")->value_name("dir"), "gitea repo base dir")
		("gitea_token", po::value<std::string>(&gitea_token)->value_name("token"), "gitea api token")
		("gitea_api_path", po::value<std::string>(&gitea_api_path)->value_name("url"), "default http://localhost:3000 cmall will use this url to manage gitea")
		("gitea_template_user", po::value<std::string>(&gitea_template_user)->default_value("admin")->value_name("user"), "template for user's default repo")
		("gitea_template_reponame", po::value<std::string>(&gitea_template_reponame)->value_name("repo"), "template for user's default repo")
		("netease_secret_id", po::value<std::string>(&netease_secret_id)->value_name("netease_secret_id"), "netease secret id")
		("netease_secret_key", po::value<std::string>(&netease_secret_key)->value_name("netease_secret_key"), "netease secret key")
		("netease_business_id", po::value<std::string>(&netease_business_id)->value_name("netease_business_id"), "netease business id")
		("tencent_id", po::value<std::string>(&tencent_secret_id)->value_name("tencent_secret_id"), "tencent secret id")
		("tencent_key", po::value<std::string>(&tentcent_secret_key)->value_name("tencent_secret_key"), "tencent secret key")
		("cert", po::value<std::string>(&cert_file)->value_name("file"), "ssl cert file")
		("key", po::value<std::string>(&key_file)->value_name("file"), "ssl private key file")
		("site_name", po::value<std::string>(&site_name)->value_name("desc")->default_value("cmall"), "site name")
		("config", po::value<std::string>(&config)->value_name("cmall.conf"), "Load config options from file.")
		;

	try
	{
		// 解析命令行.
		po::variables_map vm;
		po::store(
			po::command_line_parser(argc, argv)
			.options(desc)
			.style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
			.run()
			, vm);
		po::notify(vm);

		// 输出版本信息.
		LOG_INFO << version_info();

		// 帮助输出.
		if (vm.count("help") || argc == 1)
		{
			std::cout << desc;
			co_return EXIT_SUCCESS;
		}

		if (vm.count("config"))
		{
			if (!std::filesystem::exists(config))
			{
				LOG_ERR << "No such config file: " << config;
				co_return EXIT_FAILURE;
			}

			LOG_DBG << "Load config file: " << config;
			auto cfg = po::parse_config_file(config.c_str(), desc, false);
			po::store(cfg, vm);
			po::notify(vm);
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
	cfg.repo_root = repo_root;
	cfg.gitea_api = gitea_api_path;
	cfg.gitea_template_user = gitea_template_user;
	cfg.gitea_template_reponame = gitea_template_reponame;
	cfg.gitea_admin_token = gitea_token;
	cfg.site_name = site_name;
	cfg.netease_business_id = netease_business_id;
	cfg.netease_secret_key = netease_secret_key;
	cfg.netease_secret_id = netease_secret_id;
	cfg.tencent_secret_id = tencent_secret_id;
	cfg.tencent_secret_key = tentcent_secret_key;

	cmall::cmall_service xsrv(ios, cfg);

	using namespace boost::asio::experimental::awaitable_operators;

	if (!co_await xsrv.load_repos())
		co_return EXIT_FAILURE;

	// 初始化ws acceptors.
	co_await xsrv.init_ws_acceptors();

	#if defined(BOOST_WINDOWS_API) || defined (BOOST_ASIO_HAS_IO_URING)

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
	#endif

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
