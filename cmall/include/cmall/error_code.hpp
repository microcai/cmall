
#pragma once

#include <boost/system/error_code.hpp>

namespace cmall::error{

	enum errc_t
	{
		/// wake_up called
		internal_server_error = 1,
		invalid_json,
		quota_limited,
	};

	class cmall_category : public boost::system::error_category
	{
	public:
		const char *name() const noexcept { return "cmall"; }
		std::string message(int ev) const;
	};

	const boost::system::error_category& error_category();


	inline boost::system::error_code make_error_code(errc_t e)
	{
		return boost::system::error_code(static_cast<int>(e), error_category());
	}

}
