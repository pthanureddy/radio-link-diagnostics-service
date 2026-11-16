#pragma once

#include <charconv>
#include <stdexcept>
#include <string>
#include <system_error>

namespace radio_link {

inline int parse_cli_integer(const std::string& text, const std::string& option,
                             int minimum, int maximum) {
    int value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
        || value < minimum || value > maximum) {
        throw std::invalid_argument(option + " must be an integer in ["
            + std::to_string(minimum) + ", " + std::to_string(maximum) + "]");
    }
    return value;
}

}  // namespace radio_link
