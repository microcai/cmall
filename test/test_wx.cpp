

#include "stdafx.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/beast.hpp>

#include "services/wxpay_service.hpp"

std::string testkey;
std::string test_cert;
std::string test_api_key;
std::string test_appid;
std::string test_mchid;

awaitable<int> co_main(int argc, char** argv, boost::asio::io_context& ios)
{
    services::wxpay_service wxpay_service(testkey, test_cert, test_api_key, test_appid, test_mchid, "https://cmall.chschain.com/api/wx/notify");

    std::cerr << co_await wxpay_service.get_pay_object("123") << std::endl;

	co_return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
	int main_return;

	boost::asio::io_context ios;

	boost::asio::co_spawn(ios, co_main(argc, argv, ios), [&](std::exception_ptr e, int ret){
		if (e)
			std::rethrow_exception(e);
		main_return = ret;
		ios.stop();
	});
	ios.run();
	return main_return;
}
