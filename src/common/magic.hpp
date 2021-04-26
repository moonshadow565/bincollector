#pragma once
#include <cstddef>
#include <span>
#include <string_view>

struct Magic {
    std::string_view magic = {};
    std::u8string_view extension = {};
    std::size_t offset = 0;

    template<std::size_t SM, std::size_t SE>
    [[nodiscard]] constexpr Magic(char const(&magic)[SM], char8_t const(&extension)[SE], std::size_t offset = 0) noexcept
        : magic({magic, SM - 1}), extension({extension, SE - 1}), offset(offset)
    {}

    [[nodiscard]] static std::u8string_view find(std::span<char const> data) noexcept;
};

