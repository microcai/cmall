//
// Copyright (C) 2020 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <exception>
#include <optional>
#include <thread>
#include <boost/beast.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/json.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <type_traits>

#include "fmt_extra.hpp"

std::string gen_uuid();

void set_thread_name(const char* name);
void set_thread_name(std::thread* thread, const char* name);

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

using ws = websocket::stream<tcp::socket>;
using boost::multiprecision::cpp_int;
using boost::multiprecision::cpp_dec_float_50;
using boost::multiprecision::cpp_dec_float_100;

template<class ... T> inline constexpr bool always_false = false;

boost::posix_time::ptime make_localtime(std::string_view time) noexcept;

template<class Iterator>
std::string to_hexstring(Iterator start, Iterator end, std::string const& prefix)
{
	typedef std::iterator_traits<Iterator> traits;
	static_assert(sizeof(typename traits::value_type) == 1, "to hex needs byte-sized element type");

	static char const* hexdigits = "0123456789abcdef";
	size_t off = prefix.size();
	std::string hex(std::distance(start, end) * 2 + off, '0');
	hex.replace(0, off, prefix);

	for (; start != end; start++)
	{
		hex[off++] = hexdigits[(*start >> 4) & 0x0f];
		hex[off++] = hexdigits[*start & 0x0f];
	}

	return hex;
}

// 将cpp_int转为十六进制字符串, 可指定长度.
inline std::string to_hexstring_impl(cpp_int const& data, std::streamsize digits)
{
	std::string ret;
	if (data < 0)
	{
		cpp_int tmp(-data);
		ret = tmp.str(digits, std::ios_base::hex);
	}
	else {
		ret = data.str(digits, std::ios_base::hex);
	}

	auto prefix_length = digits - static_cast<int>(ret.size());
	if (prefix_length > 0)
	{
		std::string prefix(prefix_length, '0');
		return prefix + ret;
	}

	if (data < 0)
	{
		ret = "-" + ret;
	}

	return ret;
}

template<class T>
std::string to_hexstring(T const& data, std::streamsize digits = 64, std::string prefixed = "")
{
	if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
		std::is_same_v<std::decay_t<T>, std::vector<uint8_t>>)
	{
		auto tmp = to_hexstring(data.begin(), data.end(), "");
		int64_t num = static_cast<int64_t>(digits) - static_cast<int64_t>(tmp.size());
		if (num > 0)
			tmp = std::string(num, '0') + tmp;
		return prefixed + tmp;
	}
	else if constexpr (std::is_same_v<std::decay_t<T>, cpp_int> ||
		std::is_same_v<std::decay_t<T>, int64_t> ||
		std::is_same_v<std::decay_t<T>, uint64_t>)
	{
		// 将cpp_int或int64_t, uint64_t转为16进制字符串, 可指定前辍和长度.
		if (data < 0)
			return "-" + prefixed + to_hexstring_impl(0 - data, digits);
		return prefixed + to_hexstring_impl(data, digits);
	}
	else
	{
		static_assert(always_false<T>, "only string or std::vector<uint8_t>");
	}
}

std::string to_string(const boost::posix_time::ptime& t);

inline std::string to_string(cpp_dec_float_50 bvalue){
	std::stringstream oss;

	oss << std::fixed << std::setprecision(std::numeric_limits<cpp_dec_float_50>::digits) << bvalue;

	std::string result = oss.str();

	if (result.find('.') !=  std::string::npos)
	{
		boost::trim_right_if(result, boost::algorithm::is_any_of("0"));
		if ( * result.rbegin() ==  '.')
			result.resize(result.length() -1 );
	}

	return result;
}

inline std::string to_string(cpp_dec_float_100 bvalue){
	std::stringstream oss;

	oss << std::fixed << std::setprecision(std::numeric_limits<cpp_dec_float_100>::digits) << bvalue;

	std::string result = oss.str();

	if (result.find('.') !=  std::string::npos)
	{
		boost::trim_right_if(result, boost::algorithm::is_any_of("0"));
		if ( * result.rbegin() ==  '.')
			result.resize(result.length() -1 );
	}

	return result;
}

inline std::string fill_hexstring(const std::string& str)
{
	int prefix_length = 64 - static_cast<int>(str.size());
	if (prefix_length > 0)
	{
		std::string prefix(prefix_length, '0');
		return prefix + str;
	}

	return str;
}

template <typename E>
constexpr auto to_underlying(E e) noexcept
{
	// c++23 会有std::to_underlying
	return static_cast<std::underlying_type_t<E>>(e);
}

template <typename T>
requires std::integral<T> || std::floating_point<T>
std::optional<T> parse_number(std::string_view str, int base = 10)
{
	try {
		if constexpr (std::is_integral_v<T>)
		{
			if (std::is_signed_v<T>) 
			{
				return static_cast<T>(std::stoll(str.data(), nullptr, base));
			} 
			else 
			{
				return static_cast<T>(std::stoull(str.data(), nullptr, base));
			}
		}
		else
		{
			return static_cast<T>(std::stold(str.data(), nullptr));
		}
	} catch (const std::exception& e) {
		return {};
	}
}