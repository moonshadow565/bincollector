#pragma once
#include <string>
#include <string_view>
#include <fmt/format.h>

#define fmt_print(out, fmtstr, ...) (out << to_std_string(fmt::format(FMT_STRING(fmtstr), __VA_ARGS__)) << std::flush)

inline std::u8string to_lower(std::u8string str) {
    for (auto& c: str) {
        if (c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }
    }
    return str;
}

inline std::string to_std_string(std::u8string const& s) {
    return { s.begin(), s.end() };
}

inline std::u8string from_std_string(std::string const& s) {
    return { s.begin(), s.end() };
}
