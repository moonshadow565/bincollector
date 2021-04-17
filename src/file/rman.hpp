#pragma once
#include <file/base.hpp>
#include <file/rman/manifest.hpp>

namespace file {
    struct FileRMAN final : IFile {
        FileRMAN(rman::FileInfo const& info, fs::path const& base);

        std::u8string find_name(HashList& hashes) override;
        std::uint64_t find_hash(HashList& hashes) override;
        std::u8string find_extension(HashList& hashes) override;
        std::u8string get_link() override;
        std::size_t size() const override;
        std::shared_ptr<IReader> open() override;

        static List list_manifest(rman::RMANManifest const& manifest, fs::path const& cdn);
    private:
        struct Reader;
        rman::FileInfo info_;
        fs::path base_;
        std::weak_ptr<Reader> reader_;
    };
}
