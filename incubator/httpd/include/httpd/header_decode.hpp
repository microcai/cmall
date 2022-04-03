
#pragma once

#include <optional>
#include <string_view>
#include <cstdint>
#include <cstring>
#include <ctime>

namespace httpd {

    struct BytesRange
    {
        std::uint64_t begin, end;
    };


	bool parse_gmt_time_fmt(std::string_view date_str, time_t* output);

    std::optional<BytesRange> parse_range(std::string_view range);
}
