//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <type_traits>
#include <tuple>
#include <any>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <chrono>
#include <variant>
#include <exception>
#include <system_error>
#include <stdexcept>
#include <thread>
#include <algorithm>
#include <numeric>
#include <optional>
#include <random>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#include <boost/core/ignore_unused.hpp>

#pragma warning(push)
#pragma warning(disable: 4702 4459)

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif

#include <boost/asio/post.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/signal_set.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__


#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warray-bounds"
#endif // _MSC_VER

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
#endif // _MSC_VER

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/find.hpp>

#pragma warning(pop)

#include <boost/thread.hpp>
#include <boost/smart_ptr/local_shared_ptr.hpp>
#include <boost/smart_ptr/make_local_shared.hpp>


#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <boost/signals2.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
namespace multiprecision = boost::multiprecision;
#include <boost/circular_buffer.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifndef ENABLE_LOGGER
#define ENABLE_LOGGER
#endif

#pragma warning(push)
#pragma warning(disable: 4244 4127 4702)

#include "dmall/logging.hpp"

#pragma warning(pop)

#include "dmall/url_parser.hpp"
#include "dmall/time_clock.hpp"

// #include "ethash/ethash.hpp"
// #include "ethash/keccak.hpp"
// #include "ethash/progpow.hpp"

// using ethash::hash256;
// using ethash::hash512;
// using ethash::hash1024;
// using ethash::hash2048;

#pragma warning(push)
#pragma warning(disable: 4267)

// #include "libethcore/Common.h"
// #include "libdevcore/CommonJS.h"
// #include "libethcore/TransactionBase.h"

// #include "libdevcore/CommonJS.h"
// #include "libdevcrypto/SecretStore.h"
// #include "libdevcrypto/CryptoPP.h"
// #include "libdevcrypto/Common.h"

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif // _MSC_VER

// #include "cryptopp/base32.h"

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
#endif // _MSC_VER

#pragma warning(pop)

#include "dmall/io_context_pool.hpp"

#define APP_NAME "dmall"
// #define HTTPD_VERSION_STRING	     APP_NAME "/1.0"

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

using ws = websocket::stream<tcp::socket>;
using boost::multiprecision::cpp_int;
using boost::multiprecision::cpp_dec_float_50;
using boost::multiprecision::cpp_dec_float_100;
using dmall::io_context_pool;
using timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;

template<class ... T> inline constexpr bool always_false = false;

template <typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

#include "dmall/misc.hpp"
