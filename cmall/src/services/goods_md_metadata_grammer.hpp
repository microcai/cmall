
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <iostream>
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4244 4459)
#endif // _MSC_VER

namespace qi = boost::spirit::qi;

struct KV
{
	std::string k;
	std::string v;
};

BOOST_FUSION_ADAPT_STRUCT(
	KV,
	(std::string, k)
	(std::string, v)
)

template<typename T>
struct magic_vector : public std::vector<T>
{
public:
	magic_vector& operator += (const T& element)
	{
		std::vector<T>::push_back(element);
		return *this;
	}

	magic_vector& operator += (const magic_vector<T>& other_vector)
	{
		for (const T& element : other_vector)
			std::vector<T>::push_back(element);
		return *this;
	}
};

struct goods_description {
	std::string title;
	std::string price;
	magic_vector<std::string> picture;
	magic_vector<std::string> keywording;
	std::string description;

	magic_vector<KV> rest_meta;

	// 赋值是合并的操作.
	goods_description& operator = (const goods_description& other)
	{
		if (title.empty())
			title = other.title;
		if (price.empty())
			price = other.price;
		picture += other.picture;
		keywording += other.keywording;
		if (description.empty())
			description = other.description;

		rest_meta += other.rest_meta;
		return *this;
	}

	// 赋值是合并的操作.
	goods_description& operator = (const std::string&)
	{
		return *this;
	}
};


BOOST_FUSION_ADAPT_STRUCT(
	goods_description,
	(std::string, title)
	(std::string, price)
	(std::string, description)
	(magic_vector<std::string>, picture)
	(magic_vector<std::string>, keywording)
	(magic_vector<KV>, rest_meta)
)

template <typename Iterator>
struct goods_description_grammer : qi::grammar<Iterator, goods_description()>
{
	goods_description_grammer() : goods_description_grammer::base_type(document)
	{
		using qi::debug;
		using namespace boost::phoenix;

		document = document_sperator>> lines [ qi::_val = qi::_1 ] >> document_sperator;

		lines = line [ qi::_val = qi::_1 ] >> *( line [ qi::_val = qi::_1 ] );

		line = title_line  [ at_c<0>(qi::_val) = qi::_1 ]
			| price_line  [ at_c<1>(qi::_val) = qi::_1 ]
			| description_line  [ at_c<2>(qi::_val) = qi::_1 ]
			| picture_line [ at_c<3>(qi::_val) += qi::_1 ]
			| keyword_line [ at_c<4>(qi::_val) += qi::_1 ]
			| pair_line [ at_c<5>(qi::_val) += qi::_1 ];

		document_sperator = qi::lit("---") >> newline;

		title_line		 = qi::lit("title") >> *qi::space >> ':' >> *qi::space >> value[qi::_val = qi::_1] >> newline;
		price_line		 = qi::lit("price") >> *qi::space >> ':' >> *qi::space >> value[qi::_val = qi::_1] >> newline;
		description_line = qi::lit("description") >> *qi::space >> ':' >> *qi::space >> value[qi::_val = qi::_1] >> newline;
		picture_line	 = qi::lit("picture") >> *qi::space >> ':' >> *qi::space >> value[qi::_val = qi::_1] >> newline;
		keyword_line	 = qi::lit("keyword") >> *qi::space >> ':' >> *qi::space >> value[qi::_val = qi::_1] >> newline;

		pair_line = key [ at_c<0>(qi::_val) = qi::_1 ] >> ':' >> *qi::space >> value [ at_c<1>(qi::_val) = qi::_1 ] >> newline;
		key = qi::lexeme[ +(qi::char_ - ':' - '-' - ' ') ];
		value = qi::lexeme[ +(qi::char_ - '\n') ];

		newline = qi::lit("\r\n") | qi::lit('\n');
	};

	qi::rule<Iterator, goods_description()> document;

	qi::rule<Iterator> document_sperator, newline;

	qi::rule<Iterator, goods_description()> lines, line;
	qi::rule<Iterator, KV()> pair_line;

	qi::rule<Iterator, std::string()> key, value;

	qi::rule<Iterator, std::string()> title_line, price_line, description_line, picture_line, keyword_line;
};

inline std::optional<goods_description> parse_goods_metadata(const std::string_view& document) noexcept
{
	goods_description ast;

	goods_description_grammer<std::string_view::const_iterator> gramer;

	auto first = document.begin();

	std::cerr << "parse:" << document;

	bool r = boost::spirit::qi::parse(first, document.end(), gramer, ast);

	if (!r)
	{
		return {};
	}

	return ast;
}
