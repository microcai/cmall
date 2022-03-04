
#pragma once
#define BOOST_SPIRIT_UNICODE 1 
//#define BOOST_SPIRIT_DEBUG 1
//#define BOOST_SPIRIT_DEBUG_OUT std::cerr

#include <iostream>
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

struct KV
{
	std::string k;
	std::string nocare;
	std::string v;
};

BOOST_FUSION_ADAPT_STRUCT(
	KV,
	(std::string, k)
	(std::string, v)
)

struct comma_kv
{
	std::vector<KV> kvs;
};

namespace qi = boost::spirit::qi;

BOOST_FUSION_ADAPT_STRUCT(
	comma_kv,
	(std::vector<KV>, kvs)
)

template <typename Iterator>
struct comma_kv_grammer : qi::grammar<Iterator, comma_kv()>
{
	comma_kv_grammer() : comma_kv_grammer::base_type(document)
	{
		using qi::debug;
		using namespace boost::phoenix;

		document = document_sperator >> lines [ at_c<0>(qi::_val) = qi::_1 ] >> document_sperator;

		lines = line >> *( line );
		line = -pair >> newline;

		document_sperator = qi::lit("---") >> newline;

		pair = key [ at_c<0>(qi::_val) = qi::_1 ] >> ':' >> *space >> value [ at_c<1>(qi::_val) = qi::_1 ];

		key = qi::lexeme[ +(qi::char_ - ':' - '-' - ' ') ];
		value = qi::lexeme[ +(qi::char_ - '\n') ];

		newline = qi::lit("\r\n") | qi::lit('\n');

		space = qi::lit(" ")|qi::lit("\t");

		BOOST_SPIRIT_DEBUG_NODE(document);


		BOOST_SPIRIT_DEBUG_NODE(document_sperator);
		BOOST_SPIRIT_DEBUG_NODE(newline);
		BOOST_SPIRIT_DEBUG_NODE(space);
		BOOST_SPIRIT_DEBUG_NODE(lines);
		BOOST_SPIRIT_DEBUG_NODE(line);
		BOOST_SPIRIT_DEBUG_NODE(pair);
		BOOST_SPIRIT_DEBUG_NODE(key);
		BOOST_SPIRIT_DEBUG_NODE(value);

	};

	qi::rule<Iterator, comma_kv()> document;

	qi::rule<Iterator> document_sperator, newline, space;

	qi::rule<Iterator, std::vector<KV>()> lines;
	qi::rule<Iterator, KV()> pair, line;
	qi::rule<Iterator, std::string()> key, value;
};

struct goods_description {
	std::string title;
	std::string price;
	std::string picture;
	std::string description;
};

class KV_as_map
{
	std::vector<KV> kv;
public:
	KV_as_map(const std::vector<KV>& kv) : kv(kv) {}

	std::string operator[] (std::string key) const
	{
		auto result_it = std::find_if(kv.begin(), kv.end(), [&](auto v) {return v.k == key;});
		if (result_it != kv.end())
			return result_it->v;
		return "";
	}
};

std::optional<goods_description> parse_comma_kv(const std::string& document)
{
	comma_kv ast;

	comma_kv_grammer<decltype(document.begin())> gramer;

	auto first = document.begin();

	std::cerr << "parse:" << document;

	bool r = boost::spirit::qi::parse(first, document.end(), gramer, ast);

	if (!r)
	{
		return {};
	}

	goods_description ret;
	ret.description = KV_as_map(ast.kvs)["description"];
	ret.title = KV_as_map(ast.kvs)["title"];
	ret.price = KV_as_map(ast.kvs)["price"];
	ret.picture = KV_as_map(ast.kvs)["picture"];
	return ret;
}
