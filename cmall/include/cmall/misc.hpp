//
// Copyright (C) 2020 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <boost/json.hpp>
#include <sstream>

void set_thread_name(const char* name);
void set_thread_name(boost::thread* thread, const char* name);

std::string gen_uuid();
bool make_listen_endpoint(const std::string& address, tcp::endpoint& endp, boost::system::error_code& ec);

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

std::string to_string(const boost::posix_time::ptime& t);

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

inline int gen_random_int(int start, int end)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(start, end);

	return dis(gen);
}

inline std::string gen_unique_string(const unsigned int max_str_len)
{
	static const char szAcsiiTable[] = {
		'a', 'b', 'c', 'd', 'e',
		'f', 'g', 'h', 'i', 'j',
		'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y',
		'z', '1', '2', '3', '4',
		'5', '6', '7', '8', '9',
		'0'
	};
	static const int table_len = sizeof(szAcsiiTable) / sizeof(char);

	std::string str;
	for (unsigned int i = 0; i < max_str_len; i++) {

		int index = gen_random_int(0, table_len - 1);
		str.append(1, szAcsiiTable[index]);
	}

	return str;
}

template<typename Printable>
inline std::string output(const Printable& s)
{
	std::stringstream ss;
	ss << s;
	return ss.str();
}

namespace cmall {
	using namespace boost;

	namespace detail {
		template<typename T>
		void json_append(json::array& v, T t)
		{
			v.emplace_back(t);
		}

		template<typename T, class ... Args>
		void json_append(json::array& v, T first, Args ... params)
		{
			v.emplace_back(first);
			json_append(v, params...);
		}
	}

	template <class ... Args>
	json::value make_rpc_json(const std::string& method, Args ... params)
	{
		json::array params_array;
		detail::json_append(params_array, params...);

		return json::value{
			{"jsonrpc", "2.0" },
			{"method", method },
			{"params", params_array },
			{"id", rand()}
		};
	}

	inline json::value make_rpc_json(const std::string& method, const json::value& params)
	{
		return json::value{
			{"jsonrpc", "2.0" },
			{"method", method },
			{"params", params },
			{"id", rand()}
		};
	}

	inline std::string json_to_string(const json::value& jv, bool lf = true)
	{
		if (lf) return json::serialize(jv) + "\n";
		return json::serialize(jv);
	}

	template <class ... Args>
	std::string make_rpc_json_string(const std::string& method, Args ... params)
	{
		json::array params_array;
		detail::json_append(params_array, params...);

		json::value tmp{
			{"jsonrpc", "2.0" },
			{"method", method },
			{"params", params_array },
			{"id", rand()}
		};

		return json_to_string(tmp, false);
	}

	class json_accessor
	{
	public:
		json_accessor(const json::object& obj)
			: obj_(obj)
		{}

		inline json::value get(char const* key, json::value default_value) const
		{
			try {
				if (obj_.contains(key))
					return obj_.at(key);
			}
			catch (const std::invalid_argument&)
			{}

			return default_value;
		}

	private:
		const json::object& obj_;
	};

	inline std::string json_as_string(const json::value& value, std::string default_value = "")
	{
		try {
			auto ref = value.as_string();
			return std::string(ref.begin(), ref.end());
		}
		catch (const std::invalid_argument&)
		{}

		return default_value;
	}
}
