#pragma once
#include <common/fs.hpp>
#include <cinttypes>
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>
#include <type_traits>

namespace file {
    struct HashList {
        std::uint64_t find_hash_by_name(std::u8string name);
        std::u8string find_name_by_hash(std::uint64_t hash);
        std::u8string find_extension_by_name(std::u8string name);
        std::u8string find_extension_by_hash(std::uint64_t hash);
        std::u8string find_extension_by_data(std::uint64_t hash, std::span<char const> data);

        inline bool read_names_list(fs::path const& path) {
            auto const result = read_list(names, path);
            rebuild_extension_list();
            return result;
        }
        inline void write_names_list(fs::path const& path) {
            write_list(names, path);
        }

        inline bool read_extensions_list(fs::path const& path) {
            return read_list(extensions, path);
        }
        inline void write_extensions_list(fs::path const& path)  {
            write_list(extensions, path);
        }
    private:
        std::unordered_map<std::uint64_t, std::u8string> names;
        std::unordered_map<std::uint64_t, std::u8string> extensions;

        void rebuild_extension_list();
        static bool read_list(std::unordered_map<std::uint64_t, std::u8string>& list, fs::path const& path);
        static void write_list(std::unordered_map<std::uint64_t, std::u8string> const& list, fs::path const& path);
    };
}
