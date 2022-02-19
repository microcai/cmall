
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
	}
	return "error message";
}
