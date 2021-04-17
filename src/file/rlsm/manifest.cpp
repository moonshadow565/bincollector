#include <common/bt_error.hpp>
#include <common/fs.hpp>
#include <file/rlsm/manifest.hpp>
#include <unordered_set>
#include <unordered_map>

using namespace rlsm;

template<typename T> requires(std::is_trivial_v<T>)
static inline T read_value(std::span<char const>& data) {
    T result;
    bt_assert(data.size() >= sizeof(T));
    std::memcpy(&result, data.data(), sizeof(T));
    data = data.subspan(sizeof(T));
    return result;
}

template<typename T> requires(std::is_trivial_v<T>)
static inline std::vector<T> read_list(std::span<char const>& data) {
    auto const count = read_value<std::uint32_t>(data);
    bt_assert(data.size() >= sizeof(T) * count);
    auto result = std::vector<T>{};
    std::memcpy(result.data(), data.data(), sizeof(T) * count);
    data = data.subspan(sizeof(T) * count);
    return result;
}

RLSMManifest RLSMManifest::read(std::span<char const> data) {
    auto result = RLSMManifest{};
    result.header = bt_rethrow(read_value<RLSMHeader>(data));
    bt_assert(result.header.magic == std::array { u8'R', u8'L', u8'S', u8'M' });
    result.folders = bt_rethrow(read_list<RLSMFolder>(data));
    result.files = bt_rethrow(read_list<RLSMFile>(data));
    auto const namesCount = bt_rethrow(read_value<std::uint32_t>(data));
    auto const namesSize = bt_rethrow(read_value<std::uint32_t>(data));
    bt_assert(data.size() >= namesSize);
    bt_assert(namesSize > 0 && data[namesSize - 1] == u8'\0');
    for (auto i = data.data(); i != data.data() + namesSize; ++i) {
        bt_assert(result.names.size() < namesCount);
        auto const length = ::strlen(data.data());
        result.names.emplace_back(i, i + length);
        i += length;
    }
    bt_assert(result.names.size() >= result.header.project_name);
    return result;
}

std::vector<FileInfo> RLSMManifest::list_files() const {
    auto result = std::vector<FileInfo>{};
    auto dir_parents = std::unordered_map<std::uint32_t, std::uint32_t>{};
    auto file_parents = std::unordered_map<std::uint32_t, std::uint32_t>{};
    for (std::uint32_t p = 0; p != folders.size(); ++p) {
        auto const& parent = folders[p];
        bt_assert(names.size() >= parent.name);
        bt_assert(folders.size() >= parent.folders_start + parent.folders_count);
        bt_assert(folders.size() >= parent.files_start + parent.files_count);
        for (std::uint32_t c = parent.folders_start; c != parent.folders_start + parent.folders_count; ++c) {
            dir_parents[c] = p;
        }
        for (std::uint32_t c = parent.files_start; c != parent.files_start + parent.files_count; ++c) {
            file_parents[c] = p;
        }
    }
    for (std::uint32_t c = 0; c != files.size(); ++c) {
        bt_assert(names.size() >= files[c].name);
    }
    auto visited = std::unordered_set<std::uint32_t>();
    for (auto const& file: files) {
        auto& file_info = result.emplace_back(file, names[file.name]);
        visited.clear();
        while (auto p = file_parents[file.name]) {
            bt_assert(!visited.contains(p));
            visited.insert(p);
            file_info.name = names[p] + u8"/" + file_info.name;
            p = dir_parents[p];
        }
    }
    return result;
}
