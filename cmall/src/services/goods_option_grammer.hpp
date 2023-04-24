
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <iostream>
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

#include "magic_vector.hpp"

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4244 4459)
#endif // _MSC_VER

namespace qi = boost::spirit::qi;

struct selection
{
	std::string select_name;
	std::string select_price;
};

BOOST_FUSION_ADAPT_STRUCT(
	selection,
	(std::string, select_name)
	(std::string, select_price)
)

struct goods_option {
	std::string option_name; // 选配
	magic_vector<selection> selections;
};

BOOST_FUSION_ADAPT_STRUCT(
	goods_option,
	(std::string, option_name)
	(magic_vector<selection>, selections)
)

typedef magic_vector<goods_option> goods_options;

template <typename Iterator>
struct goods_options_grammer : qi::grammar<Iterator, goods_options()>
{
	goods_options_grammer() : goods_options_grammer::base_type(document)
	{
		using qi::debug;
		using namespace boost::phoenix;

		document = *(+newline) >>  option_block[qi::_val += qi::_1] >> * ( +newline >> option_block[qi::_val += qi::_1] );

		option_block = qi::lit('#') >> option_name[ at_c<0>(qi::_val) = qi::_1 ] >> newline >> selections [ at_c<1>(qi::_val) = qi::_1 ] ;

		selections = select[qi::_val += qi::_1] >> * ( +newline >> select[qi::_val += qi::_1] ) >> *(+newline);

		select = select_no_price | select_with_price;

		select_with_price = key [ at_c<0>(qi::_val) = qi::_1 ]
				>> qi::lit(' ') >> *qi::space >>  value [ at_c<1>(qi::_val) = qi::_1 ] >> *qi::space;

		select_no_price = key [ at_c<0>(qi::_val) = qi::_1 ] ;

		key = simple_key | quoted_key;

		simple_key = qi::lexeme[ +(qi::char_ - '#' - ':' - ' ' - '\"' -'\r' - '\n') ];

		quoted_key = qi::lit('\"') >> simple_key >> qi::lit('\"');

		value = qi::lexeme[ +(qi::char_ - ' ' - '\n') ];

		option_name = qi::lexeme[ +(qi::char_ - '\n' -'\r' ) ];
		newline = qi::lit("\r\n") | qi::lit('\n');
	};

	qi::rule<Iterator, goods_options()> document;

	qi::rule<Iterator, goods_option()> option_block;

	qi::rule<Iterator, std::string()> option_name;

	qi::rule<Iterator, magic_vector<selection>()> selections;
	qi::rule<Iterator, selection()> select;
	qi::rule<Iterator, selection()> select_with_price;
	qi::rule<Iterator, selection()> select_no_price;

	qi::rule<Iterator> newline;

	qi::rule<Iterator, std::string()> key, value, simple_key, quoted_key;
};

inline std::optional<goods_options> parse_goods_option(std::string_view document) noexcept
{
	goods_options ast;

	goods_options_grammer<std::string_view::const_iterator> gramer;

	auto first = document.begin();

	std::cerr << "parse:" << document;

	bool r = boost::spirit::qi::parse(first, document.end(), gramer, ast);

	if (!r)
	{
		return {};
	}

	return ast;
}
