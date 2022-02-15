#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>

#include <array>
#include <fstream>
#include <streambuf>
#include <tuple>
#include <utility>

#ifdef __linux__
#include <sys/resource.h>
#include <systemd/sd-daemon.h>

#ifndef HAVE_UNAME
#define HAVE_UNAME
#endif

#elif _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "cmall/internal.hpp"
#include "cmall/simple_http.hpp"
#include "cmall/version.hpp"
#include "cmall/cmall.hpp"

int platform_init() {
#if defined(WIN32) || defined(_WIN32)
	/* Disable the "application crashed" popup. */
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

#if defined(DEBUG) || defined(_DEBUG)
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
	if (setrlimit(RLIMIT_NOFILE, &of) < 0) {
		perror("setrlimit for nofile");
	}
	struct rlimit core_limit;
	core_limit.rlim_cur = RLIM_INFINITY;
	core_limit.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
		perror("setrlimit for coredump");
	}

	/* Set the stack size programmatically with setrlimit */
	rlimit rl;
	int result = getrlimit(RLIMIT_STACK, &rl);
	if (result == 0) {
		const rlim_t stack_size = 100 * 1024 * 1024;
		if (rl.rlim_cur < stack_size) {
			rl.rlim_cur = stack_size;
			result		= setrlimit(RLIMIT_STACK, &rl);
			if (result != 0)
				perror("setrlimit for stack size");
		}
	}
#endif

	std::ios::sync_with_stdio(false);

	return 0;
}

std::string version_info() {
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

	if (std::string(un.sysname) == "Linux" && ma_ver < 3) {
		std::cerr << "you are running a very very OLD kernel. please upgrade your system" << std::endl;
	}

#elif defined(__APPLE__)
	os_name = "Drawin";
#else
	os_name = "unknow";
#endif // _WIN32

	std::ostringstream oss;
	oss << "cmall version: v" << cmall_VERSION << ", " << cmall_GIT_REVISION << " built on " << __DATE__ << " "
		<< __TIME__ << " runs on " << os_name << ", " << BOOST_COMPILER;

	return oss.str();
}

int main(int argc, char** argv) {
	std::vector<std::string> http_listens;

	std::string db_name;
	std::string db_host;
	std::string db_user;
	std::string db_passwd;
	unsigned short db_port;

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Help message.")
		("version", "Current version.")

		("http", po::value<std::vector<std::string>>(&http_listens)->multitoken(), "http_listens.")

		("db_name", po::value<std::string>(&db_name)->default_value("cmall"), "Database name.")
		("db_host", po::value<std::string>(&db_host)->default_value("127.0.0.1"), "Database host.")
		("db_port", po::value<unsigned short>(&db_port)->default_value(5432), "Database port.")
		("db_user", po::value<std::string>(&db_user)->default_value("postgres"), "Database user.")
		("db_passwd", po::value<std::string>(&db_passwd)->default_value("postgres"), "Database password.");

	try {
		// 解析命令行.
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		// 输出版本信息.
		LOG_INFO << version_info();

		// 帮助输出.
		if (vm.count("help") || argc == 1) {
			std::cout << desc;
			return EXIT_SUCCESS;
		}
	} catch (const std::exception& e) {
		std::cerr << "exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	auto concurrency = boost::thread::hardware_concurrency() + 2;
	io_context_pool ios{ concurrency };

	boost::asio::signal_set terminator_signal(ios.get_io_context());
	terminator_signal.add(SIGINT);
	terminator_signal.add(SIGTERM);
//#ifdef __linux__
//	signal(SIGPIPE, SIG_IGN);
//#endif
#if defined(SIGQUIT)
	terminator_signal.add(SIGQUIT);
#endif // defined(SIGQUIT)

	cmall::server_config cfg;
	cmall::db_config dbcfg;

	dbcfg.host_		= db_host;
	dbcfg.password_ = db_passwd;
	dbcfg.user_		= db_user;
	dbcfg.dbname_	= db_name;
	dbcfg.port_		= db_port;

	cfg.dbcfg_		  = dbcfg;
	cfg.http_listens_ = http_listens;

	cmall::cmall_service dsrv{ ios, cfg };

	dsrv.start();

	// 处理中止信号.
	terminator_signal.async_wait([&ios, &dsrv](const boost::system::error_code&, int) {
		LOG_DBG << "terminator is called!";
		dsrv.stop();
		LOG_DBG << "server stopped!";
		ios.stop();
		LOG_DBG << "ios stopped!";
	});

	ios.run(5);

	LOG_DBG << "cmall system exiting...";
	return EXIT_SUCCESS;
}
