#pragma once
#include <file/base.hpp>
#include <file/rman/manifest.hpp>

namespace file {
    struct FileRMAN final : IFile {
        FileRMAN(rman::FileInfo const& info,
                 fs::path const& base,
                 std::shared_ptr<Location> location);

        std::u8string find_name(HashList& hashes) override;
        std::uint64_t find_hash(HashList& hashes) override;
        std::u8string find_extension(HashList& hashes) override;
        std::u8string get_link() override;
        std::size_t size() const override;
        std::u8string id() const override;
        std::shared_ptr<Location> location() const override;
        std::shared_ptr<IReader> open() override;
        bool is_wad() override;

    private:
        struct Reader;
        rman::FileInfo info_;
        fs::path base_;
        std::weak_ptr<Reader> reader_;
        std::shared_ptr<Location> location_;
    };

    struct ManagerRMAN : IManager {
        ManagerRMAN(std::shared_ptr<IReader> source,
                    fs::path const& cdn,
                    std::set<std::u8string> const& langs,
                    std::shared_ptr<Location> source_location);

        std::vector<std::shared_ptr<IFile>> list() override;
    private:
        fs::path cdn_;
        std::shared_ptr<Location> location_;
        std::vector<rman::FileInfo> files_;
    };
}
