
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/promise.hpp>

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/circular_buffer.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/composite_key.hpp>

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/tag.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/find.hpp>

#include <boost/json.hpp>

#include <boost/system.hpp>

#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/format.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/process.hpp>
#include <boost/process/extend.hpp>
#include <boost/scope_exit.hpp>
#include <boost/core/ignore_unused.hpp>

#include <boost/regex.hpp>

#include <boost/variant2.hpp>
#include <boost/smart_ptr/local_shared_ptr.hpp>
#include <boost/smart_ptr/make_local_shared.hpp>
#include <boost/signals2.hpp>

#include <algorithm>
#include <any>
#include <chrono>
#include <clocale>
#include <concepts>
#include <exception>

#include <filesystem>
#include <fstream>
#include <functional>

#include <iostream>
#include <iterator>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <utility>
#include <optional>
#include <random>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <version>


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

#include <odb/database.hxx>
#include <odb/query.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/transaction.hxx>

#include <odb/pgsql/database.hxx>
#include <odb/pgsql/traits.hxx>

#ifdef __linux__
#  include <sys/resource.h>
#  include <systemd/sd-daemon.h>
#  include <sys/utsname.h>
#elif _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>
#endif
