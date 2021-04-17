#include <file/base.hpp>
#include <file/rlsm/manifest.hpp>
#include <file/wad/wad.hpp>

namespace file {
    struct FileWAD final : IFile {
        FileWAD(wad::Entry const& info, std::shared_ptr<IReader> source);

        std::u8string find_name(HashList& hashes) override;
        std::uint64_t find_hash(HashList& hashes) override;
        std::u8string find_extension(HashList& hashes) override;
        std::u8string get_link() override;
        std::size_t size() const override;
        std::shared_ptr<IReader> open() override;

        static List list_wad(std::shared_ptr<IReader> source);
    private:
        struct Reader;
        struct ReaderUncompressed;
        struct ReaderZSTD;
        struct ReaderZLIB;

        wad::Entry info_;
        std::shared_ptr<IReader> source_;
        std::weak_ptr<Reader> reader_;
        std::u8string link_;
    };
}
