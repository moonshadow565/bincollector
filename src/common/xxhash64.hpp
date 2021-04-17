#pragma once
#pragma once
#include <bit>
#include <cstddef>
#include <cinttypes>
#include <string_view>

constexpr uint64_t XXH64(std::u8string_view str, uint64_t seed = 0) noexcept {
    constexpr std::uint64_t Prime1 = 11400714785074694791U;
    constexpr std::uint64_t Prime2 = 14029467366897019727U;
    constexpr std::uint64_t Prime3 =  1609587929392839161U;
    constexpr std::uint64_t Prime4 =  9650029242287828579U;
    constexpr std::uint64_t Prime5 =  2870177450012600261U;
    constexpr auto Char = [](char8_t c) constexpr -> std::uint64_t {
        return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    };
    constexpr auto HalfBlock = [Char](char8_t const* data) constexpr -> std::uint64_t {
        return Char(data[0]) | (Char(data[1]) << 8) | (Char(data[2]) << 16) | (Char(data[3]) << 24);
    };
    constexpr auto Block = [HalfBlock](char8_t const* data) constexpr -> std::uint64_t {
        return HalfBlock(data) | (HalfBlock(data + 4) << 32);
    };
    std::uint64_t result = static_cast<std::uint64_t>(str.size());
    if (str.size() >= 32u) {
        std::uint64_t s1 = seed + Prime1 + Prime2;
        std::uint64_t s2 = seed + Prime2;
        std::uint64_t s3 = seed;
        std::uint64_t s4 = seed - Prime1;
        while (str.size() >= 32) {
            s1 = std::rotl(s1 + Block(str.data()) * Prime2, 31) * Prime1;
            s2 = std::rotl(s2 + Block(str.data() + 8) * Prime2, 31) * Prime1;
            s3 = std::rotl(s3 + Block(str.data() + 16) * Prime2, 31) * Prime1;
            s4 = std::rotl(s4 + Block(str.data() + 24) * Prime2, 31) * Prime1;
            str = str.substr(32);
        }
        std::uint64_t tmp = std::rotl(s1,  1) + std::rotl(s2,  7) + std::rotl(s3, 12) + std::rotl(s4, 18);
        tmp ^= std::rotl(s1 * Prime2, 31) * Prime1;
        tmp = tmp * Prime1 + Prime4;
        tmp ^= std::rotl(s2 * Prime2, 31) * Prime1;
        tmp = tmp * Prime1 + Prime4;
        tmp ^= std::rotl(s3 * Prime2, 31) * Prime1;
        tmp = tmp * Prime1 + Prime4;
        tmp ^= std::rotl(s4 * Prime2, 31) * Prime1;
        tmp = tmp * Prime1 + Prime4;
        result += tmp;
    } else {
        result += seed + Prime5;
    }
    while (str.size() >= 8) {
        result ^= std::rotl(Block(str.data()) * Prime2, 31) * Prime1;
        result = std::rotl(result, 27) * Prime1 + Prime4;
        str = str.substr(8);
    }
    while (str.size() >= 4) {
        result ^= HalfBlock(str.data()) * Prime1;
        result = std::rotl(result, 23) * Prime2 + Prime3;
        str = str.substr(4);
    }
    while (!str.empty()) {
        result ^= Char(str.front()) * Prime5;
        result = std::rotl(result, 11) * Prime1;
        str = str.substr(1);
    }
    result ^= result >> 33;
    result *= Prime2;
    result ^= result >> 29;
    result *= Prime3;
    result ^= result >> 32;
    return result;
}
