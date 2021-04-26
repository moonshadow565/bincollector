#include <file/base.hpp>
#include <file/rlsm/manifest.hpp>
#include <file/wad/wad.hpp>

namespace file {
    struct FileWAD final : IFile {
        FileWAD(wad::EntryInfo const& info, std::shared_ptr<IReader> source, std::u8string const& source_id);
        FileWAD(wad::EntryInfo const& info, std::shared_ptr<IFile> source);

        std::u8string find_name(HashList& hashes) override;
        std::uint64_t find_hash(HashList& hashes) override;
        std::u8string find_extension(HashList& hashes) override;
        std::u8string get_link() override;
        std::size_t size() const override;
        std::u8string id() const override;
        std::shared_ptr<IReader> open() override;
        bool is_wad() override;

    private:
        struct Reader;
        struct ReaderUncompressed;
        struct ReaderZSTD;
        struct ReaderZLIB;

        wad::EntryInfo info_;
        std::shared_ptr<IReader> source_;
        std::weak_ptr<Reader> reader_;
        std::u8string link_;
        std::u8string source_id_;
    };

    struct ManagerWAD : IManager {
        ManagerWAD(std::shared_ptr<IFile> source);
        ManagerWAD(std::shared_ptr<IReader> source, std::u8string const& source_id);

        std::vector<std::shared_ptr<IFile>> list() override;
    private:
        std::vector<wad::EntryInfo> entries_;
        std::shared_ptr<IReader> source_;
        std::u8string source_id_;
    };
}
