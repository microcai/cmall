//
// Copyright (C) 2019 Jack.
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

#include <odb/pgsql/traits.hxx>
#include <odb/pgsql/details/endian-traits.hxx>

#include "cmall/db.hpp"

inline std::string numeric_to_string(const cpp_numeric& number)
{
	std::stringstream oss;

	oss << std::fixed << std::setprecision(std::numeric_limits<cpp_numeric>::digits) << number;

	std::string result = oss.str();

	if (result.find('.') != std::string::npos)
	{
		boost::trim_right_if(result, boost::algorithm::is_any_of("0"));
		if (*result.rbegin() == '.')
			result.resize(result.length() - 1);
	}

	return result;
}

namespace odb {
	namespace pgsql {

		template <>
		class value_traits<std::vector<int>, id_string>
		{
		public:
			typedef std::vector<int> value_type;
			typedef value_type query_type;
			typedef details::buffer image_type;

			static void
				set_value(value_type& v,
					const details::buffer& b,
					std::size_t n,
					bool is_null)
			{
				v.clear();

				if (!is_null)
				{
					char c;
					std::string s(b.data(), n);
					// std::cerr << "in: " << s << std::endl;
					std::istringstream is(s);

					is >> c; // '{'

					for (c = static_cast<char> (is.peek());
						c != '}';
						is >> c)
					{
						v.push_back(int());
						is >> v.back();
					}
				}
			}

			static void
				set_image(details::buffer& b,
					std::size_t& n,
					bool& is_null,
					const value_type& v)
			{
				is_null = false;
				std::ostringstream os;

				os << '{';

				for (value_type::const_iterator i(v.begin()),
					e(v.end());
					i != e;)
				{
					os << *i;

					if (++i != e)
						os << ',';
				}

				os << '}';

				const std::string& s(os.str());
				// std::cerr << "out: " << s << std::endl;
				n = s.size();

				if (n > b.capacity())
					b.capacity(n);

				std::memcpy(b.data(), s.c_str(), n);
			}
		};

		template <>
		class value_traits<std::vector<std::string>, id_string>
		{
		public:
			typedef std::vector<std::string> value_type;
			typedef value_type query_type;
			typedef details::buffer image_type;

			static void
				set_value(value_type& v,
					const details::buffer& b,
					std::size_t n,
					bool is_null)
			{
				v.clear();

				if (!is_null)
				{
					char c;
					std::string s(b.data(), n);
					// std::cerr << "in: " << s << std::endl;
					std::istringstream is(s);

					is >> c; // '{'
					std::string temp;

					do {
						is >> c;
						if (c == '}' || c == ',') {
							if (!temp.empty()) {
								v.push_back(temp);
								temp.clear();
							}
							if (c == '}')
								break;
						}
						else {
							temp.push_back(c);
						}
					} while (!is.eof());
				}
			}

			static void
				set_image(details::buffer& b,
					std::size_t& n,
					bool& is_null,
					const value_type& v)
			{
				is_null = false;
				std::ostringstream os;

				os << '{';

				for (value_type::const_iterator i(v.begin()),
					e(v.end());
					i != e;)
				{
					os << *i;

					if (++i != e)
						os << ',';
				}

				os << '}';

				const std::string& s(os.str());
				// std::cerr << "out: " << s << std::endl;
				n = s.size();

				if (n > b.capacity())
					b.capacity(n);

				std::memcpy(b.data(), s.c_str(), n);
			}
		};

		template <>
		class value_traits<cpp_int, id_string>
		{
		public:
			typedef cpp_int value_type;
			typedef value_type query_type;
			typedef details::buffer image_type;

			static void
				set_value(value_type& v,
					const details::buffer& b,
					std::size_t n,
					bool is_null)
			{
				if (!is_null)
				{
					std::string s(b.data(), n);
					v = value_type(s);
				}
			}

			static void
				set_image(details::buffer& b,
					std::size_t& n,
					bool& is_null,
					const value_type& v)
			{
				is_null = false;
				std::string s = v.str();
				n = s.size();
				if (n > b.capacity())
					b.capacity(n);
				std::memcpy(b.data(), s.c_str(), n);
			}
		};

		template <>
		class value_traits<cpp_numeric, id_string>
		{
		public:
			typedef cpp_numeric value_type;
			typedef value_type query_type;
			typedef details::buffer image_type;

			static void
				set_value(value_type& v,
					const details::buffer& b,
					std::size_t n,
					bool is_null)
			{
				if (!is_null)
				{
					std::string s(b.data(), n);
					v = value_type(s);
				}
			}

			static void
				set_image(details::buffer& b,
					std::size_t& n,
					bool& is_null,
					const value_type& v)
			{
				is_null = false;
				std::string s = numeric_to_string(v);
				n = s.size();
				if (n > b.capacity())
					b.capacity(n);
				std::memcpy(b.data(), s.c_str(), n);
			}
		};
	}
}
