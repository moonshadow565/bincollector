#pragma once
#include <array>
#include <cinttypes>
#include <string>
#include <span>
#include <vector>
#include <fmt/format.h>

namespace rlsm {
    struct RLSMVersion {
        std::array<std::uint8_t, 4> raw;
        inline operator std::u8string() const {
            return fmt::format(u8"{}.{}.{}.{}", raw[3], raw[2], raw[1], raw[0]);
        }
    };

    struct RLSMHeader {
        std::array<char8_t, 4> magic;
        std::uint16_t version_major;
        std::uint16_t version_minor;
        std::uint32_t project_name;
        RLSMVersion release_version;
    };

    struct RLSMFolder {
        std::uint32_t name;
        std::uint32_t folders_start;
        std::uint32_t folders_count;
        std::uint32_t files_start;
        std::uint32_t files_count;
    };

    struct RLSMFile {
        std::uint32_t name;
        RLSMVersion version;
        std::array<std::uint8_t, 16> checksum;
        std::uint32_t deploy_mode;
        std::uint32_t size_uncompressed;
        std::uint32_t size_compressed;
        std::uint32_t date_low;
        std::uint32_t date_hi;
    };

    struct FileInfo : RLSMFile {
        std::u8string name;
    };

    struct RLSMManifest {
        RLSMHeader header;
        std::vector<RLSMFolder> folders;
        std::vector<RLSMFile> files;
        std::vector<std::u8string> names;

        std::vector<FileInfo> list_files() const;
        static RLSMManifest read(std::span<char const> data);
    };
}
