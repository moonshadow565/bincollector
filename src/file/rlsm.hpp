#pragma once
#include <file/base.hpp>
#include <file/rlsm/manifest.hpp>

namespace file {
    struct FileRLSM final : IFile {
        FileRLSM(rlsm::FileInfo const& info, fs::path const& base);

        std::u8string find_name(HashList& hashes) override;
        std::uint64_t find_hash(HashList& hashes) override;
        std::u8string find_extension(HashList& hashes) override;
        std::u8string get_link() override;
        std::size_t size() const override;
        std::shared_ptr<IReader> open() override;

        static List list_manifest(rlsm::RLSMManifest const& manifest, fs::path const& cdn);
    private:
        struct Reader;
        rlsm::FileInfo info_;
        fs::path path_;
        std::weak_ptr<Reader> reader_;
    };
}
