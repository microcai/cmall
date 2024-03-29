
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

using boost::asio::awaitable;

namespace services
{
	using cpp_numeric = boost::multiprecision::cpp_dec_float_100;

	namespace weixin {

		struct tencent_microapp_pay_cfg_t{
			std::string rsa_key_;
			std::string rsa_cert_;
			std::string apiv3_key_;
			std::string notify_url_;
			std::string appid_;
			std::string mchid_;
			std::string appSecret_;
		};

		enum class pay_status{
			SUCCESS,// 支付成功
			REFUND, // 转入退款
			NOTPAY, // 未支付
			CLOSED, // 已关闭
			REVOKED, //已撤销（仅付款码支付会返回）
			USERPAYING, // 用户支付中（仅付款码支付会返回）
			PAYERROR, // 支付失败（仅付款码支付会返回）
			QUERY_FAILED, // 查询失败
		};

		struct notify_message
		{
			std::string out_trade_no;
			pay_status trade_state;
			cpp_numeric order_amount;
			cpp_numeric payed_amount;
			boost::posix_time::ptime success_time;
		};

		notify_message tag_invoke(const boost::json::value_to_tag<notify_message>&, const boost::json::value& jv);
	}

	struct wxpay_service_impl;
	class wxpay_service
	{
	public:
		wxpay_service(boost::asio::io_context& io, weixin::tencent_microapp_pay_cfg_t cfg);
		~wxpay_service();

		void reinit(weixin::tencent_microapp_pay_cfg_t cfg);

		// 获取 openid
		awaitable<std::string> get_wx_openid(std::string jscode);

		// 获取 openid
		awaitable<std::string> gongzhonghao_get_wx_openid(std::string jscode);

		// 获取 prepay_id
		awaitable<std::string> get_prepay_id(std::string sub_mchid, std::string out_trade_no, cpp_numeric amount, std::string goods_description, std::string payer_openid, bool profit_sharing = false);

		// 这个返回的接口, 客户端调用 wx.requestPayment(OBJECT) 发起支付.
		// 在小程序的前端页面, 调用 payobj = await cmallsdk.get_wx_pay_object(prepay_id) 获取 payobj
		// 然后调用 wx.requestPayment(payobj), 小程序就会调用微信支付了.
		awaitable<boost::json::object> get_pay_object(std::string prepay_id);

		awaitable<weixin::notify_message> decode_notify_message(std::string_view notify_body, std::string_view Wechatpay_Timestamp, std::string_view Wechatpay_Nonce, std::string_view Wechatpay_Signature);

		awaitable<void> download_latest_wxpay_cert();

	private:
		const wxpay_service_impl& impl() const;
		wxpay_service_impl& impl();

		std::array<char, 2048> obj_stor;
	};
}
