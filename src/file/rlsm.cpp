#include <common/bt_error.hpp>
#include <common/mmap.hpp>
#include <file/hashlist.hpp>
#include <file/raw.hpp>
#include <file/rlsm.hpp>
#include <file/rlsm/manifest.hpp>

using namespace file;

struct FileRLSM::Reader final : IReader {
    Reader(rlsm::FileInfo const& info, fs::path const& path)
        : info_(info), path_(path)
    {
        bt_trace(u8"path: {}", path.generic_u8string());
        bt_rethrow(data_.open(path).unwrap());
    }

    std::size_t size() const override {
        return info_.size_uncompressed;
    }

    std::span<char const> read(std::size_t offset, std::size_t size) override {
        bt_assert(data_.span().size() >= offset + size);
        bt_assert(size + offset == 0 || data_.data()); // empty files don't need to be open
        return data_.span().subspan(offset, size);
    }
private:
    rlsm::FileInfo info_;
    fs::path path_;
    MMap<char const> data_;
};

FileRLSM::FileRLSM(rlsm::FileInfo const& info, fs::path const& base)
    : info_(info), path_(base / u8"release" / static_cast<std::u8string>(info_.version) / u8"files" / info_.name)
{}

std::u8string FileRLSM::find_name([[maybe_unused]] HashList& hashes) {
    return info_.name;
}

std::uint64_t FileRLSM::find_hash(HashList& hashes) {
    return hashes.find_hash_by_name(info_.name);
}

std::u8string FileRLSM::find_extension(HashList& hashes) {
    return hashes.find_extension_by_name(info_.name);
}

std::u8string FileRLSM::get_link() {
    return {};
}

std::size_t FileRLSM::size() const {
    return info_.size_uncompressed;
}

std::u8string FileRLSM::id() const {
    std::uint64_t c[2];
    std::memcpy(&c, &info_.checksum, 16);
    auto result = fmt::format(u8"{:016x}{:016x}", c[1], c[0]);
    std::reverse(result.begin(), result.end());
    return fmt::format(u8"{}.md5", result);
}

std::shared_ptr<IReader> FileRLSM::open() {
    if (auto result = reader_.lock()) {
        return result;
    } else {
        reader_ = (result = std::make_shared<Reader>(info_, path_));
        return result;
    }
}

bool FileRLSM::is_wad() {
    auto const& name = info_.name;
    return name.ends_with(u8".wad") || name.ends_with(u8".client") || name.ends_with(u8".mobile");
}

ManagerRLSM::ManagerRLSM(std::shared_ptr<IReader> source, fs::path const& cdn, std::set<std::u8string> const& langs) {
    auto manifest = rlsm::RLSMManifest::read(source->read());
    base_ = cdn / u8"projects" / manifest.names[manifest.header.project_name];
    files_ = manifest.list_files();
    (void)langs;
}

std::vector<std::shared_ptr<IFile>> ManagerRLSM::list() {
    auto result = std::vector<std::shared_ptr<IFile>>{};
    result.reserve(files_.size());
    for (auto const& entry: files_) {
        result.emplace_back(std::make_shared<FileRLSM>(entry, base_));
    }
    return result;
}

ManagerSLN::ManagerSLN(std::shared_ptr<IReader> source, fs::path const& cdn, std::set<std::u8string> const& langs) {
    auto manifest = sln::SLNManifest::read(source->read());
    for (auto const& project: manifest.list_projects()) {
        if (!project.has_locale(langs)) {
            continue;
        }
        auto const path = cdn / u8"projects" / project.name / u8"releases" / project.version / u8"releasemanifest";
        auto project_source = FileRAW::make_reader(path);
        managers_.emplace_back(std::make_unique<ManagerRLSM>(project_source, cdn, std::set<std::u8string>{}));
    }
}

std::vector<std::shared_ptr<IFile>> ManagerSLN::list() {
    auto result = std::vector<std::shared_ptr<IFile>>{};
    for (auto const& manager: managers_) {
        auto files = manager->list();
        result.insert(result.end(), files.begin(), files.end());
    }
    return result;
}
