
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <iostream>
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

#include "services/magic_vector.hpp"

#include "utils/logging.hpp"

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4244 4459)
#endif // _MSC_VER

namespace qi = boost::spirit::qi;

struct selection_item
{
	std::string option;
	std::string select;
};

BOOST_FUSION_ADAPT_STRUCT(
	selection_item,
	(std::string, option)
	(std::string, select)
)

typedef magic_vector<selection_item> selection_descrption;

template <typename Iterator>
struct selection_descrption_grammer : qi::grammar<Iterator, selection_descrption()>
{
	selection_descrption_grammer() : selection_descrption_grammer::base_type(document)
	{
		using qi::debug;
		using namespace boost::phoenix;

		document = item[qi::_val += qi::_1] >> *item[qi::_val += qi::_1];

		item = option_name[ at_c<0>(qi::_val) = qi::_1 ] >> qi::lit(':') >> value [ at_c<1>(qi::_val) = qi::_1 ] >> qi::lit(';');

		value = qi::lexeme[ +(qi::char_ - ';' - '\n') ];

		option_name = qi::lexeme[ +(qi::char_ - '\n' -'\r' - ':' - ';') ];
	};

	qi::rule<Iterator, selection_descrption()> document;
	qi::rule<Iterator, selection_item()> item;
	qi::rule<Iterator, std::string()> option_name;
	qi::rule<Iterator, std::string()> value;
};

inline std::optional<selection_descrption> parse_selection_description_string(std::string_view document) noexcept
{
	selection_descrption ast;

	selection_descrption_grammer<std::string_view::const_iterator> gramer;

	auto first = document.begin();

	LOG_DBG << "parse selection:" << document;

	bool r = boost::spirit::qi::parse(first, document.end(), gramer, ast);

	if (!r)
	{
		return {};
	}

	return ast;
}
