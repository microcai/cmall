
#pragma once

#include <functional>
#include <string_view>
#include <string>

namespace md {

    typedef std::function<std::string(std::string)> url_replacer;

    std::string markdown_transpile(std::string_view original, url_replacer);
}
