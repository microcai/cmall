
#include "cmall/error_code.hpp"

const boost::system::error_category& cmall::error::error_category()
{
	static cmall::error::cmall_category cmall_error_cat;

	return cmall_error_cat;
}

std::string cmall::error::cmall_category::message(int ev) const
{
	return "error message";
}