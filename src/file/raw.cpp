#include <common/bt_error.hpp>
#include <common/mmap.hpp>
#include <file/hashlist.hpp>
#include <file/raw.hpp>

using namespace file;

struct FileRAW::Reader final : IReader {
    Reader(fs::path const& path) : path_(path) {
        bt_trace(u8"path: {}", path.generic_u8string());
        bt_rethrow(data_.open(path).unwrap());
    }

    std::size_t size() const override {
        return data_.size();
    }

    std::span<char const> read(std::size_t offset, std::size_t size) override {
        bt_assert(data_.span().size() >= offset + size);
        bt_assert(size + offset == 0 || data_.data()); // empty files don't need to be open
        return data_.span().subspan(offset, size);
    }
private:
    fs::path path_;
    MMap<char const> data_;
};

FileRAW::FileRAW(std::u8string const& name, fs::path const& base)
    : name_(name), path_(base / name)
{}

std::u8string FileRAW::find_name([[maybe_unused]] HashList& hashes) {
    return name_;
}

std::uint64_t FileRAW::find_hash(HashList& hashes) {
    return hashes.find_hash_by_name(name_);
}

std::u8string FileRAW::find_extension(HashList& hashes) {
    return hashes.find_extension_by_name(name_);
}

std::u8string FileRAW::get_link() {
    return {};
}

std::size_t FileRAW::size() const {
    std::error_code ec = {};
    auto const size = fs::file_size(path_, ec);
    bt_assert(ec == std::error_code{});
    return static_cast<std::size_t>(size);
}

std::u8string FileRAW::id() const {
    return {};
}

std::shared_ptr<IReader> FileRAW::open() {
    if (auto result = reader_.lock()) {
        return result;
    } else {
        reader_ = (result = std::make_shared<Reader>(path_));
        return result;
    }
}

bool FileRAW::is_wad() {
    auto const ext = path_.extension().generic_u8string();
    return ext == u8".wad" || ext == u8".client" || ext == u8".mobile";
}

std::shared_ptr<IReader> FileRAW::make_reader(fs::path const& path) {
    return std::make_shared<Reader>(path);
}

ManagerRAW::ManagerRAW(fs::path const& base) : base_(base) {}

std::vector<std::shared_ptr<IFile>> ManagerRAW::list() {
    auto result = std::vector<std::shared_ptr<IFile>>{};
    for (auto const& entry: fs::recursive_directory_iterator(base_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto path = fs::relative(entry.path(), base_).generic_u8string();
        result.emplace_back(std::make_shared<FileRAW>(path, base_));
    }
    return result;
}
