
#include "stdafx.hpp"

#include "cmall/error_code.hpp"

const boost::system::error_category& cmall::error::error_category()
{
	static cmall::error::cmall_category cmall_error_cat;

	return cmall_error_cat;
}

std::string cmall::error::cmall_category::message(int ev) const
{
	switch (ev)
	{
		case internal_server_error:
			return (const char*) u8"服务器内部错误";
		case unknown_method:
			return (const char*) u8"rpc 方法未知";
		case session_needed:
			return (const char*) u8"需要先获取session";
		case invalid_verify_code:
			return (const char*) u8"验证码错误";
		case login_required:
			return (const char*) u8"需要登录才能操作";
		case merchant_user_required:
			return (const char*) u8"需要商户登录才能操作";
		case admin_user_required:
			return (const char*) u8"需要管理员登录才能操作";
		case not_implemented:
			return (const char*) u8"本功能还没实现";
		case recipient_id_out_of_range:
			return (const char*) u8"收件地址id错误";
		case duplicate_operation:
			return (const char*) u8"请勿重复操作";
		case already_exist:
			return (const char*) u8"记录已存在";
		case already_a_merchant:
			return (const char*) u8"不用申请啦, 已经是商户了";
		case order_not_found:
			return (const char*) u8"订单未找到";
		case invalid_params:
			return (const char*) u8"参数错误";
		case cart_goods_not_found:
			return (const char*) u8"购物车商品未找到";
		case already_in_cart:
			return (const char*) u8"已在购物车中";
		case merchant_vanished:
			return (const char*) u8"商户蜜汁消失";
		case no_payment_script_supplyed:
			return (const char*) u8"没提供 getpayurl.js";
		case no_paycheck_script_spplyed:
			return (const char*) u8"未提供 checkpay.js";
		case merchant_has_no_payments_supported:
			return (const char*) u8"商户未做好收款准备";
		case user_not_found:
			return (const char*) u8"未找到用户";
		case user_banned:
			return (const char*) u8"该手机无法接收验证码";
		case not_in_sudo_mode:
			return (const char*) u8"未处于替身模式";
		case cannot_sudo:
			return (const char*) u8"无法进入替身模式";
		case kv_store_key_not_found:
			return (const char*) u8"key不存在";
	}
	return "error message";
}
