
#include <string>
#include <string_view>

#include "./md_transpile.hpp"

// 本函数, 遇到每个 ![image](url) 标记, 就调用 replacer 获取更新后的地址.
std::string md::markdown_transpile(std::string_view original, url_replacer replacer)
{
    std::string converted;
    converted.reserve(original.length() * 1.5);

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
