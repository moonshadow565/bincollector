#pragma once
#include <array>
#include <cinttypes>
#include <span>
#include <string_view>
#include <vector>

namespace wad {
    struct Header {
        std::array<char8_t, 2> magic;
        std::uint8_t versionMajor;
        std::uint8_t versionMinor;
    };
    struct HeaderV1 : Header {
        std::uint16_t entry_offset;
        std::uint16_t entry_size;
        std::uint32_t entry_count;
    };
    struct HeaderV2 : Header {
        std::array<std::uint8_t, 84> signature;
        std::array<std::uint8_t, 8> checksum;
        std::uint16_t entry_offset;
        std::uint16_t entry_size;
        std::uint32_t entry_count;
        constexpr operator HeaderV1() const noexcept {
            return HeaderV1 { *this, entry_offset, entry_size, entry_count };
        }
    };
    struct HeaderV3 : Header {
        std::uint8_t signature[256];
        std::array<std::uint8_t, 8> checksum;
        static constexpr std::uint16_t entry_offset = 272;
        static constexpr std::uint16_t entry_size = 32;
        std::uint32_t entry_count;
        constexpr operator HeaderV1() const noexcept {
            return HeaderV1 { *this, entry_offset, entry_size, entry_count };
        }
    };

    struct Entry {
        enum class Type : std::uint8_t {
            Uncompressed,
            ZlibCompressed,
            FileRedirection,
            ZStandardCompressed
        };
        std::uint64_t path;
        std::uint32_t offset;
        std::uint32_t size_compressed;
        std::uint32_t size_uncompressed;
        Type type;
        std::uint8_t pad[3];

        void extract(std::span<char const> src, std::span<char> dst) const noexcept;
    };
    struct EntryV1 : Entry {};
    struct EntryV2 : EntryV1 {};
    struct EntryV3 : EntryV2 {
        std::array<std::uint8_t, 8> checksum;
    };

    struct EntryList {
        [[nodiscard]] inline EntryList() = default;
        [[nodiscard]] std::size_t read_header_size(std::span<char const> data);
        [[nodiscard]] std::size_t read_toc_size(std::span<char const> data);
        [[nodiscard]] std::vector<Entry> read_entries(std::span<char const> data);
    private:
        HeaderV1 header = {};
    };
}
