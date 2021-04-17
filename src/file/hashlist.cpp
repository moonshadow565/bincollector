#include <common/mmap.hpp>
#include <common/bt_error.hpp>
#include <common/xxhash64.hpp>
#include <common/magic.hpp>
#include <file/hashlist.hpp>
#include <optional>
#include <charconv>
#include <vector>
#include <fmt/format.h>

using namespace file;

template <typename T>
static constexpr auto read_num (std::span<char8_t const>& iter) {
    auto const start = reinterpret_cast<char const*>(iter.data());
    auto const end = start + iter.size();
    T num;
    auto const result = std::from_chars(start, end, num, 16);
    bt_assert(result.ec == std::errc{});
    iter = iter.subspan(static_cast<std::size_t>(result.ptr - start));
    return num;
};

static constexpr auto read_char (std::span<char8_t const>& iter) {
    bt_assert(iter.size() > 0);
    auto const result = iter.front();
    iter = iter.subspan(1);
    return result;
}

static constexpr auto read_string (std::span<char8_t const>& iter) {
    auto str_len = 0u;
    while (str_len != iter.size() && iter[str_len] != '\r' && iter[str_len] != '\n') {
        ++str_len;
    }
    auto result = std::u8string_view { iter.data(), str_len };
    iter = iter.subspan(str_len);
    return result;
}

bool HashList::read_list(std::unordered_map<std::uint64_t, std::u8string>& list, fs::path const& path) {
    if (!fs::exists(path)) {
        return false;
    }
    auto mmap = MMap<char8_t const>{};
    auto const open_error = mmap.open(path);
    bt_assert(!open_error);
    for (auto iter = mmap.span(); !iter.empty();) {
        if (iter.front() == '\r' || iter.front() == '\n') {
            iter = iter.subspan(1);
            continue;
        }
        auto const hash = read_num<std::uint64_t>(iter);
        auto const separator = read_char(iter);
        bt_assert(separator == ' ');
        auto const str = read_string(iter);
        bt_assert(!str.empty());
        list.insert_or_assign(hash, str);
    }
    return true;
}

template <typename T>
static constexpr auto write_num (std::span<char8_t>& iter, T value) {
    auto result = fmt::format("{:016X}", value);
    std::copy_n(result.data(), result.size(), iter.data());
    iter = iter.subspan(result.size());
}

static constexpr auto write_char(std::span<char8_t>& iter, char8_t value) {
    iter.front() = value;
    iter = iter.subspan(1);
}

static constexpr auto write_string(std::span<char8_t>& iter, std::u8string_view value) {
    std::copy_n(value.data(), value.size(), iter.data());
    iter = iter.subspan(value.size());
}

void HashList::write_list(std::unordered_map<std::uint64_t, std::u8string> const& list, fs::path const& path) {
    using pair_t = std::pair<std::uint64_t const, std::u8string>;
    auto sorted = std::vector<pair_t const*>();
    sorted.reserve(list.size());
    auto storage_size = std::size_t{};
    for (auto const& pair: list) {
        storage_size += 16 + 1 + 1;
        storage_size += pair.second.size();
        sorted.push_back(&pair);
    }
    std::sort(sorted.begin(), sorted.end(), [] (pair_t const* lhs, pair_t const* rhs) -> bool {
        return std::tie(lhs->second, lhs->first) < std::tie(rhs->second, rhs->first);
    });
    auto mmap = MMap<char8_t>{};
    auto const open_error = mmap.create(path, storage_size);
    bt_assert(!open_error);
    auto iter = mmap.span();
    for (auto pair: sorted) {
        write_num(iter, pair->first);
        write_char(iter, ' ');
        write_string(iter, pair->second);
        write_char(iter, '\n');
    }
}

void HashList::rebuild_extension_list() {
    for (auto const& [hash, name]: names) {
        if (auto e = extensions.find(hash); e == extensions.end()) {
            auto ext = fs::path(name).extension().generic_u8string();
            if (ext.empty()) {
                ext = u8".";
            }
            extensions.emplace_hint(e, hash, std::move(ext));
        }
    }
}

std::uint64_t HashList::find_hash_by_name(std::u8string name) {
    auto const hash = XXH64(name);
    if (auto n = names.find(hash); n == names.end()) {
        names.emplace_hint(n, hash, name);
    }
    if (auto e = extensions.find(hash); e == extensions.end()) {
        auto ext = fs::path(name).extension().generic_u8string();
        if (ext.empty()) {
            ext = u8".";
        }
        extensions.emplace_hint(e, hash, ext);
    }
    return hash;
}

std::u8string HashList::find_name_by_hash(std::uint64_t hash) {
    if (auto n = names.find(hash); n != names.end()) {
        return n->second;
    }
    return {};
}

std::u8string HashList::find_extension_by_name(std::u8string name) {
    auto const hash = XXH64(name);
    if (auto n = names.find(hash); n == names.end()) {
        names.emplace_hint(n, hash, name);
    }
    if (auto e = extensions.find(hash); e != extensions.end()) {
        return e->second;
    } else {
        auto ext = fs::path(name).extension().generic_u8string();
        if (ext.empty()) {
            ext = u8".";
        }
        extensions.emplace_hint(e, hash, ext);
        return ext;
    }
    return {};
}

std::u8string HashList::find_extension_by_hash(std::uint64_t hash) {
    if (auto e = extensions.find(hash); e != extensions.end()) {
        return e->second;
    }
    return {};
}

std::u8string HashList::find_extension_by_data(std::uint64_t hash, std::span<char const> data) {
    if (auto e = extensions.find(hash); e != extensions.end()) {
        return e->second;
    } else if (!data.empty()) {
        auto ext = std::u8string{Magic::find(data)};
        if (!ext.empty()) {
            extensions.emplace_hint(e, hash, ext);
            return ext;
        }
    }
    return {};
}
