
#include "stdafx.hpp"

#include <string>
#include <string_view>

#include "./md_transpile.hpp"

static int count_backticks(std::string_view::iterator first, std::string_view::iterator last)
{
	int counted = 0;
	while (*first == '`')
	{
		counted++; first++;
		if (first ==last)
			break;
	}
	return counted;
}

// 返回当前 pos 下是几个 backtick. 0 表示没 backtick， -1 表示 EOF
// -2 表示遇到换行
// 会更新 pos 到新位置
static int backtick_lexer(std::string_view::iterator& pos, std::string_view::iterator end_postiton)
{

	// 就剩下一个 ` 了，别想了。剩下 2 个也别想了.
	// 剩下 3以上个才有意义.
	if (std::distance(pos, end_postiton) <= 3)
	{
		pos = end_postiton;
		return -1;
	}

	if (*pos == '\n') {
		pos++;
		return -2;
	}

	int backticks = count_backticks(pos, end_postiton);

	if (backticks > 0)
	{
		pos += backticks;
		return backticks;
	}
	else
		pos++;
	return 0;
}

// 遇到 `, 就跳过到下一个 `
// 遇到 `` 就跳过到下一个 ``
// 遇到 ``` 就跳过到下一个 ```
// 返回跳过后的迭代器
static std::string_view::iterator skip_backtick_text(int back_tick_type, std::string_view::iterator pos, std::string_view::iterator end_postiton)
{
	int type = -1;
	do
	{
		type = backtick_lexer(pos, end_postiton);
		if (type == -1)
			return end_postiton;
		if (type == -2)
		{
			if (back_tick_type != 3)
				return pos;
		}
	} while (type != back_tick_type);
	return pos;
}

// 本函数, 遇到每个 ![image](url) 标记, 就调用 replacer 获取更新后的地址.
std::string md::markdown_transpile(std::string_view original, url_replacer replacer)
{
    std::string converted;
    converted.reserve(original.length() * 2);

	enum grammer_state
	{
		state_normal_md,
		state_meet_exclamatory,
		state_inside_image_marker_tag,
		state_expect_image_url_parenthesis,
		state_begin_image_url,
	};

    grammer_state state = state_normal_md;

    std::string image_marker_tag;
    std::string image_url;
    int url_parser_level = 0;

    auto pos = original.begin();
    while( pos != original.end())
    {
        switch (state)
        {
            case state_normal_md:
            {
                switch (*pos)
                {
                    case '!':
                        state = state_meet_exclamatory;
                        ++ pos;
                        break;
					case '`':
					{
						auto current_pos = pos;
						auto backtick_type = backtick_lexer(pos, original.end());

						if (backtick_type > 0)
						{
							pos = skip_backtick_text(backtick_type, pos, original.end());
							converted.append(current_pos, pos);
						}
						else
						{
							converted.append(current_pos, pos);
						}break;
					}
					break;
                    default:
                        converted.push_back(*pos++);
                        break;
                }
            }break;
            case state_meet_exclamatory:
            {
                switch (*pos)
                {
                    case '[':
                    {
                        state = state_inside_image_marker_tag;
                        ++ pos;
                    }break;
                    default:
                        state = state_normal_md;
                        converted.push_back('!');
                        converted.push_back(*pos++);
                        break;
                }
            }break;
            case state_inside_image_marker_tag:
            {
                switch (*pos)
                {
                    case ']':
                        state = state_expect_image_url_parenthesis;
                        ++ pos;
                        break;
                    default:
                        image_marker_tag.push_back(*pos++);
                        break;
                }
            }break;
            case state_expect_image_url_parenthesis:
            {
                switch (*pos)
                {
                case '(':
                    state = state_begin_image_url;
                    url_parser_level = 0;
                    converted.append("![");
                    converted.append(image_marker_tag);
                    converted.push_back(']');
                    image_marker_tag.clear();
                    image_url.clear();
                    ++pos;
                    break;
                case ' ':
                case '\t':
                    ++pos;
                    break;
                case '\n':
                    state = state_normal_md;
                    converted.append("![");
                    converted.append(image_marker_tag);
                    converted.append("]\n");
                    image_marker_tag.clear();
                    ++pos;
                    break;
                default:
                    ++pos;
                    break;
                }
            }
            case state_begin_image_url:
            {
                switch (*pos)
                {
                    case '(':
                        url_parser_level++;
                        image_url.push_back(*pos);
                        ++pos;
                        break;
                    case '\n':
                        state = state_normal_md;
                        converted.push_back('(');
                        converted.append(image_url);
                        converted.push_back('\n');
                        image_url.clear();
                        ++pos;
                        break;
                    case ')':
                        if (url_parser_level == 0)
                        {
                            state = state_normal_md;
                            converted.push_back('(');
                            converted.append(replacer(image_url));
                            converted.push_back(')');
                            image_url.clear();
                        }
                        else
                        {
                            url_parser_level--;
                            image_url.push_back(*pos);
                        }
                        ++pos;
                        break;
                    default:
                    {
                        image_url.push_back(*pos);
                        ++pos;
                    }
                    break;
                }
            }break;
			default:
            break;
        }
    }

    return converted;
}
