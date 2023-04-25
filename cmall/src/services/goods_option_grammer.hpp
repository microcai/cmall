
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <iostream>

#include "magic_vector.hpp"


struct goods_options_parser
{
	struct selection
	{
		std::string select_name;
		std::string select_price;
	};

	struct goods_option {
		std::string option_name; // 选配
		magic_vector<selection> selections;
	};

	typedef magic_vector<goods_option> goods_options;

	static std::optional<goods_options> parse_lines(const std::vector<std::string_view>& lines)
	{
		goods_options ret;
		bool has_block = false;
		goods_option cur_block;

		for (std::string_view l : lines)
		{
			if (l.size() < 2)
				continue;
			// skip untile meet #
			if (l[0] == '#')
			{
				if (has_block)
					ret.push_back(cur_block);
				cur_block = goods_option{};
				cur_block.option_name = boost::algorithm::trim_copy(l.substr(1));
				has_block = true;
			}
			else
			{
				std::vector<std::string_view> items;
				boost::split(items, l, boost::is_any_of(" \t"));
				if (items.size() == 1)
				{
					struct selection s;
					s.select_name = items[0];
					s.select_price = "0";
					cur_block.selections.push_back(s);
				}
				else if (items.size() == 2)
				{
					struct selection s;
					s.select_name = items[0];
					s.select_price = items[1];
					cur_block.selections.push_back(s);
				}
			}

		}

		if (has_block)
			ret.push_back(cur_block);
		if (ret.size() == 0)
			return {};
		return ret;
	}
};

inline std::optional<goods_options_parser::goods_options> parse_goods_option(std::string_view document) noexcept
{
	std::vector<std::string_view> lines;
	boost::split(lines, document, boost::is_any_of("\r\n"));

	return goods_options_parser::parse_lines(lines);
}
