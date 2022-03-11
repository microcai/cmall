
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
		case invalid_params:
			return (const char*) u8"参数错误";
		case cart_goods_not_found:
			return (const char*) u8"购物车商品未找到";
		case already_in_cart:
			return (const char*) u8"已在购物车中";
		case merchant_vanished:
			return (const char*) u8"商户蜜汁消失";
	}
	return "error message";
}
