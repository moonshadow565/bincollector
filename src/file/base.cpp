#include <common/bt_error.hpp>
#include <common/mmap.hpp>
#include <common/magic.hpp>
#include <file/base.hpp>
#include <file/raw.hpp>
#include <file/rlsm.hpp>
#include <file/rman.hpp>
#include <file/wad.hpp>
#include <digestpp.hpp>
#include <hasher.hpp>

using namespace file;

std::u8string Location::print(std::u8string_view separator) {
    std::u8string result = {};
    if (source_location) {
        result = source_location->print(separator);
    }
    if (!result.empty()) {
        result += separator;
    }
    result += path.generic_u8string();
    return result;
}

std::u8string Checksums::print(std::u8string_view key_separator, std::u8string_view list_separator) {
    std::u8string result;
    for (auto const& [key, value]: list) {
        if (!result.empty()) {
            result += list_separator;
        }
        result += key;
        result += key_separator;
        result.insert(result.end(), value.begin(), value.end());
    }
    return result;
}

IFile::~IFile() = default;

IReader::~IReader() = default;

IManager::~IManager() = default;

void IFile::extract_to(fs::path const& file_path) {
    bt_trace(u8"file_path: {}", file_path.generic_u8string());
    bt_rethrow(fs::create_directories(file_path.parent_path()));
    auto in_file = open();
    auto in_data = in_file->read();
    auto out_file = MMap<char>{};
    bt_rethrow(out_file.create(file_path, in_data.size()).unwrap());
    std::memcpy(out_file.data(), in_data.data(), in_data.size());
}

Checksums IFile::checksums() {
    auto results = Checksums{};
    if (auto link = get_link(); link.empty()) {
        auto reader = open();
        auto const data = reader->read();
        results.list[u8"md5"] = digestpp::md5().absorb(data.data(), data.size()).hexdigest();
        results.list[u8"sha1"] = digestpp::sha1().absorb(data.data(), data.size()).hexdigest();
    } else {
        results.list[u8"link"] = {link.begin(), link.end()};
    }
    return results;
}


std::shared_ptr<IManager> IManager::make(fs::path src, fs::path cdn, std::u8string remote, std::set<std::u8string> const& langs) {
    bt_trace(u8"src: {}", src.generic_u8string());
    bt_trace(u8"cdn: {}", cdn.generic_u8string());
    bt_assert(fs::exists(src));
    src = fs::absolute(src);
    if (fs::is_directory(src)) {
        return std::make_shared<ManagerRAW>(src, nullptr);
    }
    auto file = FileRAW::make_reader(src);
    auto magic = Magic::find(file->read(0, std::min(file->size(), std::size_t{16})));
    if (magic == u8".releasemanifest") {
        if (cdn.empty()) {
            //    <.> <version>     "releases"    <project>     "projects"    <realm>
            cdn = src.parent_path().parent_path().parent_path().parent_path().parent_path();
        }
        bt_assert(fs::exists(cdn));
        cdn = fs::absolute(cdn);
        return std::make_shared<ManagerRLSM>(file, cdn, langs, nullptr);
    } else if (magic == u8".solutionmanifest") {
        if (cdn.empty()) {
            //    <.> <version>     "releases"    <solution>    "projects"    <realm>
            cdn = src.parent_path().parent_path().parent_path().parent_path().parent_path();
        }
        bt_assert(fs::exists(cdn));
        cdn = fs::absolute(cdn);
        return std::make_shared<ManagerSLN>(file, cdn, langs, nullptr);
    } else if (magic == u8".manifest") {
        if (cdn.empty()) {
            //    <.> "releases"    <channel>
            cdn = src.parent_path().parent_path();
        }
        if (remote.empty()) {
            bt_assert(fs::exists(cdn));
        } else {
            bt_assert(!cdn.empty());
            bt_rethrow(fs::create_directories(cdn));
        }
        cdn = fs::absolute(cdn);
        return std::make_shared<ManagerRMAN>(file, cdn, remote, langs, nullptr);
    } else if (magic == u8".wad") {
        if (cdn.empty()) {
            //    <.>
            cdn = src.parent_path();
        }
        bt_assert(fs::exists(cdn));
        cdn = fs::absolute(cdn);
        std::u8string relative_wad_path = src.lexically_proximate(cdn).generic_u8string();
        bt_trace(u8"relative_wad_path: {}", relative_wad_path);
        bt_assert(relative_wad_path.find(u8"..") == std::u8string::npos);
        bt_assert(!relative_wad_path.empty());
        auto file_relative = std::make_shared<FileRAW>(relative_wad_path, cdn, nullptr);
        file = nullptr;
        return std::make_shared<ManagerWAD>(file_relative);
    } else {
        bt_error("Unrecognized manager format!");
    }
}


