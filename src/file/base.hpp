#pragma once
#include <common/fs.hpp>
#include <common/string.hpp>
#include <cinttypes>
#include <memory>
#include <set>
#include <span>
#include <vector>


namespace file {
    struct HashList;
    struct IFile;
    struct IManager;

    struct IReader {
        inline IReader() = default;
        IReader(IFile const&) = delete;
        IReader(IFile&&) = delete;
        IReader& operator=(IFile const&) = delete;
        IReader& operator=(IFile&&) = delete;
        virtual ~IReader() = 0;
        virtual std::size_t size() const = 0;
        virtual std::span<char const> read(std::size_t offset, std::size_t size) = 0;
        inline std::span<char const> read(std::size_t offset) {
            return read(offset, size() - offset);
        }
        inline std::span<char const> read() {
            return read(0, size());
        }
    };

    struct IFile {
        inline IFile() = default;
        IFile(IFile const&) = delete;
        IFile(IFile&&) = delete;
        IFile& operator=(IFile const&) = delete;
        IFile& operator=(IFile&&) = delete;
        virtual ~IFile() = 0;
        virtual std::u8string find_name(HashList& hashes) = 0;
        virtual std::uint64_t find_hash(HashList& hashes) = 0;
        virtual std::u8string find_extension(HashList& hashes) = 0;
        virtual std::u8string get_link() = 0;
        virtual std::size_t size() const = 0;
        virtual std::shared_ptr<IReader> open() = 0;
        virtual bool is_wad() = 0;

        void extract_to(fs::path const& file_path);
    };

    struct IManager {
        inline IManager() = default;
        IManager(IManager const&) = delete;
        IManager(IManager&&) = delete;
        IManager& operator=(IManager const&) = delete;
        IManager& operator=(IManager&&) = delete;
        virtual ~IManager() = 0;
        virtual std::vector<std::shared_ptr<IFile>> list() = 0;

        static std::shared_ptr<IManager> make(fs::path const& src, fs::path const& cdn, std::set<std::u8string> const& langs);
    };
}
