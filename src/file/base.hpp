#pragma once
#include <common/fs.hpp>
#include <common/string.hpp>
#include <cinttypes>
#include <memory>
#include <map>
#include <set>
#include <span>
#include <vector>

namespace file {
    struct HashList;
    struct IFile;
    struct IManager;

    struct Location {
        std::shared_ptr<Location> source_location;
        fs::path path;

        std::u8string print(std::u8string_view separator = u8";");
    };
    struct Checksums {
        std::map<std::u8string, std::string> list;

        std::u8string print(std::u8string_view key_separator = u8":", std::u8string_view list_separator = u8";");
    };

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
        virtual std::u8string id() const = 0;
        virtual std::shared_ptr<Location> location() const = 0;
        virtual std::shared_ptr<IReader> open() = 0;
        virtual bool is_wad() = 0;
        Checksums checksums();

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

        static std::shared_ptr<IManager> make(fs::path src, fs::path cdn, std::u8string remote, std::set<std::u8string> const& langs);
    };
}
