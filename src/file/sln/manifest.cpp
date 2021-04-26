#include "manifest.hpp"
#include <common/bt_error.hpp>
#include <common/string.hpp>
#include <charconv>

using namespace sln;

static inline std::string_view read_line(std::span<char const>& iter) {
    auto str_len = 0u;
    while (str_len != iter.size() && iter[str_len] != '\n') {
        ++str_len;
    }
    auto result = std::string_view { iter.data(), str_len };
    while (result.ends_with("\r")) {
        result.remove_suffix(1);
    }
    iter = iter.subspan(str_len);
    if (!iter.empty()) {
        iter = iter.subspan(1);
    }
    return result;
}

static inline std::size_t read_number(std::span<char const>& iter) {
    auto const line = read_line(iter);
    std::size_t result = 0;
    auto const er_ptr = std::from_chars(line.data(), line.data() + line.size(), result);
    bt_assert(er_ptr.ec == std::errc{});
    bt_assert(er_ptr.ptr == line.data() + line.size());
    return result;
}

static inline std::u8string read_string(std::span<char const>& iter) {
    auto const line = read_line(iter);
    bt_assert(!line.empty());
    return { line.begin(), line.end() };
}

SLNManifest SLNManifest::read(std::span<char const> src_data) {
    auto result = SLNManifest{};
    auto const header = read_line(src_data);
    bt_assert(header == "RADS Solution Manifest");

    result.manifest_version = read_string(src_data);
    result.solution_name = read_string(src_data);
    result.solution_version = read_string(src_data);

    auto const project_count = read_number(src_data);
    for (auto i = std::size_t{0}; i != project_count; ++i) {
        auto project_name = read_string(src_data);
        auto project_version = read_string(src_data);
        auto unknown1 = read_number(src_data);
        auto unknown2 = read_number(src_data);
        result.projects[project_name] = { project_version, unknown1, unknown2 };
    }

    auto locale_count = read_number(src_data);
    for (auto i = std::size_t{0}; i != locale_count; ++i) {
        auto locale_name = to_lower(read_string(src_data));
        auto unknown1 = read_number(src_data);
        auto project_count = read_number(src_data);
        auto projects = std::vector<std::u8string>{};
        for (auto i = std::size_t{0}; i != project_count; ++i) {
            auto project_name = read_string(src_data);
            projects.push_back(project_name);
        }
        result.locales[locale_name] = { projects, unknown1 };
    }
    return result;
}

std::vector<SLNEntryInfo> SLNManifest::list_projects() const {
    auto project_locales = std::map<std::u8string, std::set<std::u8string>> {};
    for (auto const& [locale_name, locale]: locales) {
        for (auto const& project_name: locale.projects) {
            project_locales[project_name].insert(locale_name);
        }
    }
    auto results = std::vector<SLNEntryInfo>{};
    results.reserve(projects.size());
    for (auto const& [project_name, project]: projects) {
        auto& entry = results.emplace_back(project_name, project.version);
        entry.locales = project_locales[project_name];
        if (entry.locales.size() == 0 || entry.locales.size() == locales.size()) {
            entry.locales = { u8"none" };
        }
    }
    return results;
}
