//
// Copyright (C) 2022 microcai.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <cstddef>	// std::size_t
#include <cstring>	// std::strncmp, std::memset, std::memcpy
#include <sstream>	// std::istringstream
#include <vector>	// std::vector
#include <string>	// std::string

#include <boost/multiprecision/cpp_dec_float.hpp>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/format.h>
#endif

namespace fmt {
	template <>
	struct formatter<boost::multiprecision::cpp_dec_float_50> {
		constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
			auto it = ctx.begin(), end = ctx.end();
			if (it != end)
				it++;
			if (it != end && *it != '}')
				throw format_error("invalid format");
			return it;
		}

		template <typename FormatContext>
		auto format(const boost::multiprecision::cpp_dec_float_50& p, FormatContext& ctx) -> decltype(ctx.out()) {
			std::string x = p.str(0, std::ios_base::fixed);
			auto len = x.size();
			for (auto rb = x.crbegin(); rb != x.crend(); rb++) {
				if (*rb != '0') {
					if (*rb == '.')
						len--;
					break;
				}
				len--;
			}
			x.resize(len);
			return format_to(ctx.out(), "{}", x);
		}
	};
}
