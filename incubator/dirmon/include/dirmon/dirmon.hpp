
#pragma once

#include <boost/asio.hpp>

#ifdef BOOST_OS_WINDOWS
#include "windows/windows_dirmon.hpp"
namespace dirmon {
	using dirmon = windows_dirmon;
}
#endif

