
#pragma once

#include <functional>
#include <string_view>
#include <string>

namespace mdrender {

    typedef std::function<std::string(std::string)> url_replacer;

    std::string markdown_to_html(std::string_view original, url_replacer);
}
