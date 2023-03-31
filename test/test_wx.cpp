

#include "stdafx.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/beast.hpp>

#include "services/wxpay_service.hpp"


#include "./test_key.h"

awaitable<int> co_main(int argc, char** argv, boost::asio::io_context& ios)
{
	services::weixin::tencent_microapp_pay_cfg_t cfg;
	cfg.notify_url_ = "https://cmall.chschain.com/api/wx/pay.action";
	cfg.appid_ = "wx6f3464e4046cf192";
	cfg.mchid_ = "1502765861";

	cfg.appSecret_ = test_appSecret_;
	cfg.apiv3_key_ = test_api_key;
	cfg.rsa_key_ = testkey;
	cfg.rsa_cert_ = test_cert;

    services::wxpay_service wxpay_service(ios, cfg);

	auto openid = co_await wxpay_service.get_wx_openid("0c3tz7100TR3HP1AzG200U8gfk2tz71a");

    std::cerr << "id is: " <<  openid << std::endl;

	auto prepayid = co_await wxpay_service.get_prepay_id("1641053810", "test_order_2", 1, "测试一下哈", "oh4Rj467Rx42zvafj3OmRXNJtI2M");

    std::cerr << "prepayid is: " <<  prepayid << std::endl;

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
