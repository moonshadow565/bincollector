#pragma once
#include <compare>
#include <cinttypes>
#include <string>
#include <span>
#include <vector>
#include <map>
#include <set>
#include <fmt/format.h>

namespace sln {
    struct SLNProjectEntry {
        std::u8string version = {};
        std::size_t unknown1 = {};
        std::size_t unknown2 = {};
    };

    struct SLNLocaleEntry {
        std::vector<std::u8string> projects = {};
        std::size_t unknown1 = {};
    };

    struct SLNEntryInfo {
        std::u8string name = {};
        std::u8string version = {};
        std::set<std::u8string> locales = {};

        inline bool has_locale(std::set<std::u8string> const& langs) const {
            if (langs.empty()) {
                return true;
            }
            for (auto const& lang: langs) {
                if (locales.contains(lang)) {
                    return true;
                }
            }
            return false;
        }
    };

    struct SLNManifest {
        std::u8string manifest_version = {};
        std::u8string solution_name = {};
        std::u8string solution_version = {};

        std::map<std::u8string, SLNProjectEntry> projects = {};
        std::map<std::u8string, SLNLocaleEntry> locales = {};

        static SLNManifest read(std::span<char const> src_data) ;
        std::vector<SLNEntryInfo> list_projects() const;
    };
}
