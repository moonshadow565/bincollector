#pragma once
#include <file/base.hpp>

namespace file {
    struct FileRAW final : IFile {
        FileRAW(std::u8string const& name, fs::path const& base);

        std::u8string find_name(HashList& hashes) override;
        std::uint64_t find_hash(HashList& hashes) override;
        std::u8string find_extension(HashList& hashes) override;
        std::u8string get_link() override;
        std::size_t size() const override;
        std::u8string id() const override;
        std::shared_ptr<IReader> open() override;
        bool is_wad() override;

        static std::shared_ptr<IReader> make_reader(fs::path const& path);
    private:
        struct Reader;
        std::u8string name_;
        fs::path path_;
        std::weak_ptr<Reader> reader_;
    };

    struct ManagerRAW : IManager {
        ManagerRAW(fs::path const& base);

        std::vector<std::shared_ptr<IFile>> list() override;
    private:
        fs::path base_;
    };
}
