#include <common/bt_error.hpp>
#include <common/mmap.hpp>
#include <common/magic.hpp>
#include <file/base.hpp>
#include <file/raw.hpp>
#include <file/rlsm.hpp>
#include <file/rman.hpp>
#include <file/wad.hpp>

using namespace file;

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

std::shared_ptr<IManager> IManager::make(fs::path src, fs::path cdn, std::set<std::u8string> const& langs) {
    bt_trace(u8"src: {}", src.generic_u8string());
    bt_trace(u8"cdn: {}", cdn.generic_u8string());
    bt_assert(fs::exists(src));
    bt_assert(fs::exists(cdn));
    src = fs::absolute(src).lexically_normal();
    cdn = fs::absolute(cdn).lexically_normal();
    if (fs::is_directory(src)) {
        return std::make_shared<ManagerRAW>(src);
    }
    auto file = FileRAW::make_reader(src);
    auto magic = Magic::find(file->read(0, std::min(file->size(), std::size_t{16})));
    if (magic == u8".releasemanifest") {
        return std::make_shared<ManagerRLSM>(file, cdn, langs);
    } else if (magic == u8".solutionmanifest") {
        return std::make_shared<ManagerSLN>(file, cdn, langs);
    } else if (magic == u8".manifest") {
        return std::make_shared<ManagerRMAN>(file, cdn, langs);
    } else if (magic == u8".wad") {
        std::u8string relative_wad_path = src.lexically_proximate(cdn).generic_u8string();
        bt_trace(u8"relative_wad_path: {}", relative_wad_path);
        bt_assert(relative_wad_path.find(u8"..") == std::u8string::npos);
        bt_assert(!relative_wad_path.empty());
        auto file_relative = std::make_shared<FileRAW>(relative_wad_path, cdn);
        file = nullptr;
        return std::make_shared<ManagerWAD>(file_relative);
    } else {
        bt_error("Unrecognized manager format!");
    }
}


