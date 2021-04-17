#include <common/bt_error.hpp>
#include <common/mmap.hpp>
#include <file/base.hpp>
#include <file/raw.hpp>
#include <file/rlsm.hpp>
#include <file/rman.hpp>
#include <file/wad.hpp>

using namespace file;

IFile::~IFile() = default;
IReader::~IReader() = default;

IFile::List IFile::list_path(fs::path const& path, fs::path const& cdn) {
    bt_trace(u8"path: {}", path.generic_u8string());
    bt_trace(u8"cdn: {}", cdn.generic_u8string());
    bt_assert(fs::exists(path));
    if (fs::is_directory(path)) {
        return FileRAW::list_directory(path);
    }
    auto file = FileRAW::make_reader(path);
    bt_assert(file->size() >= 4);
    if (auto const magic = file->read(0, 4); std::memcmp(magic.data(), "RLSM", 4) == 0) {
        bt_assert(fs::is_directory(cdn));
        auto manifest = rlsm::RLSMManifest::read(file->read());
        return FileRLSM::list_manifest(manifest, cdn);
    } else if (std::memcmp(magic.data(), "RMAN", 4) == 0) {
        bt_assert(fs::is_directory(cdn));
        auto manifest = rman::RMANManifest::read(file->read());
        return FileRMAN::list_manifest(manifest, cdn);
    } else {
        bt_assert(std::memcmp(magic.data(), "RW", 2) == 0);
        return FileWAD::list_wad(file);
    }
}
