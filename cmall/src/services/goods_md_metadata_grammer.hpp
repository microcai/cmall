
#pragma once
#define BOOST_SPIRIT_UNICODE 1 
//#define BOOST_SPIRIT_DEBUG 1
//#define BOOST_SPIRIT_DEBUG_OUT std::cerr

#include <iostream>
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

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
	std::string picture;
	std::string description;

	magic_vector<KV> rest_meta;

	// 赋值是合并的操作.
	goods_description& operator = (const goods_description& other)
	{
		if (title.empty())
			title = other.title;
		if (price.empty())
			price = other.price;
		if (picture.empty())
			picture = other.picture;
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
	(std::string, picture)
	(std::string, description)
	(magic_vector<KV>, rest_meta)
)

template <typename Iterator>
struct comma_kv_grammer : qi::grammar<Iterator, goods_description()>
{
	comma_kv_grammer() : comma_kv_grammer::base_type(document)
	{
		using qi::debug;
		using namespace boost::phoenix;

		document = document_sperator>> lines [ qi::_val = qi::_1 ] >> document_sperator;

		lines = line [ qi::_val = qi::_1 ] >> *( line [ qi::_val = qi::_1 ] );

		line = title_line  [ at_c<0>(qi::_val) = qi::_1 ] | price_line  [ at_c<1>(qi::_val) = qi::_1 ] | description_line  [ at_c<3>(qi::_val) = qi::_1 ] | picture_line [ at_c<2>(qi::_val) = qi::_1 ] | pair_line [ at_c<4>(qi::_val) += qi::_1 ];

		document_sperator = qi::lit("---") >> newline;

		title_line = qi::lit("title") >> * space >> ':' >> *space >> value [ qi::_val = qi::_1 ] >> newline;
		price_line = qi::lit("price") >> * space >> ':' >> *space >> value [ qi::_val = qi::_1 ] >> newline;
		description_line = qi::lit("description") >> * space >> ':' >> *space >> value [ qi::_val = qi::_1 ] >> newline;
		picture_line = qi::lit("picture") >> * space >> ':' >> *space >> value [ qi::_val = qi::_1 ] >> newline;

		pair_line = key [ at_c<0>(qi::_val) = qi::_1 ] >> ':' >> *space >> value [ at_c<1>(qi::_val) = qi::_1 ] >> newline;
		key = qi::lexeme[ +(qi::char_ - ':' - '-' - ' ') ];
		value = qi::lexeme[ +(qi::char_ - '\n') ];

		newline = qi::lit("\r\n") | qi::lit('\n');

		space = qi::lit(" ")|qi::lit("\t");
	};

	qi::rule<Iterator, goods_description()> document;

	qi::rule<Iterator> document_sperator, newline, space;

	qi::rule<Iterator, goods_description()> lines, line;
	qi::rule<Iterator, KV()> pair_line;
	qi::rule<Iterator, std::string()> key, value;

	qi::rule<Iterator, std::string()> title_line, price_line, description_line, picture_line;
};

std::optional<goods_description> parse_goods_metadata(const std::string& document)
{
	goods_description ast;

	comma_kv_grammer<decltype(document.begin())> gramer;

	auto first = document.begin();

	std::cerr << "parse:" << document;

	bool r = boost::spirit::qi::parse(first, document.end(), gramer, ast);

	if (!r)
	{
		return {};
	}

	return ast;
}
