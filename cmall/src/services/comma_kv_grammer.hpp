
#pragma once
#define BOOST_SPIRIT_UNICODE 1

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
	comma_kv_grammer() : comma_kv_grammer::base_type(comma_kvs)
	{
		using namespace boost::phoenix;

		comma_kvs = document_sperator >> +newline >> lines [ at_c<0>(qi::_val) = qi::_1 ] >> +newline >> document_sperator >> -rest_garbage;
		newline = qi::lit('\r') | qi::lit('\n');

		lines =  pair_line >> *( pair_line);
		pair_line  =  key [ at_c<0>(qi::_val) = qi::_1 ] >> ':' >> *qi::lit(' ') >> value [ at_c<1>(qi::_val) = qi::_1 ] >> +newline;
		key = qi::lexeme[ +(qi::unicode::char_ - ':' - '-' - ' ') ];
		value = qi::lexeme[ +(qi::unicode::char_ - '\n') ];

		document_sperator = qi::lit("---");

		rest_garbage = *(qi::unicode::char_);
	};

	qi::rule<Iterator, comma_kv()> comma_kvs;

	qi::rule<Iterator> document_sperator, newline;

	qi::rule<Iterator, std::vector<KV>()> lines;
	qi::rule<Iterator, KV()> pair_line;
	qi::rule<Iterator, std::string()> key, value;
	qi::rule<Iterator> rest_garbage;
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
