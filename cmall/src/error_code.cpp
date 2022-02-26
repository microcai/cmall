
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
			return (const char*) u8"jsonrpc 方法未知";
		case session_needed:
			return (const char*) u8"需要先获取session";
		case login_required:
			return (const char*) u8"需要登录才能操作";
		case merchant_user_required:
			return (const char*) u8"需要商户登录才能操作";
		case admin_user_required:
			return (const char*) u8"需要管理员登录才能操作";
		case not_implemented:
			return (const char*) u8"本功能还没实现";
	}
	return "error message";
}
