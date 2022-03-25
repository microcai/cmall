
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <optional>
#include <map>
#include <utility>
#include <string>

#include <cstring>
#include <ctime>

#include <filesystem>
#include <memory>
#include <chrono>
#include <set>

//////////////////////////////////////////////////////////////////////////

// 微软的 STL 只有 足够的新才能 include fmt 不然会报错
#if defined(__cpp_lib_format)
#include <format>
#else
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4244 4127)
#endif // _MSC_VER

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#endif

#include <fmt/ostream.h>
#include <fmt/printf.h>
#include <fmt/format.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace std {
	using namespace fmt;
}

#ifdef _MSC_VER
#	pragma warning(pop)
#endif
#endif

#include <zlib.h>
