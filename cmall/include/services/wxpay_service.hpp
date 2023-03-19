
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

using boost::asio::awaitable;

namespace services
{
	using cpp_numeric = boost::multiprecision::cpp_dec_float_100;

	struct wxpay_service_impl;
	class wxpay_service
	{
	public:
		wxpay_service(const std::string& rsa_key, const std::string& sp_appid, const std::string& sp_mchid, const std::string& notify_url);
		~wxpay_service();

		// 获取 prepay_id
		awaitable<std::string> get_prepay_id(std::string sub_mchid, std::string out_trade_no, cpp_numeric amount, std::string goods_description, std::string payer_openid);

		awaitable<boost::json::object> get_pay_object(std::string prepay_id);

	private:
		const wxpay_service_impl& impl() const;
		wxpay_service_impl& impl();

		std::array<char, 2048> obj_stor;
	};
}
