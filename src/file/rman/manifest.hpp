#pragma once
#include <array>
#include <optional>
#include <set>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>
#include <span>

namespace rman {
    enum class ChunkID : uint64_t {
        None
    };

    enum class BundleID : uint64_t {
        None
    };

    enum class LangID : uint8_t {
        None
    };

    enum class FileID : uint64_t {
        None
    };

    enum class DirID : uint64_t {
        None
    };

    enum class HashType : uint8_t {
        None,
        SHA512,
        SHA256,
        RITO_HKDF,
    };

    extern ChunkID chunk_hash(std::span<char const> data, HashType type) noexcept;

    struct RBUNChunk {
        ChunkID id;
        uint32_t uncompressed_size;
        uint32_t compressed_size;
    };

    struct RBUNFooter {
        std::array<char, 8> id_raw;
        uint32_t chunk_count;
        uint32_t unk_1;
        std::array<char8_t, 4> magic;
    };

    struct RBUNBundle {
        BundleID id;
        std::vector<RBUNChunk> chunks;

        static RBUNBundle read(std::span<char const> src_data);
    };

    struct RMANHeader {
        std::array<char8_t, 4> magic;
        uint8_t version_major;
        uint8_t version_minor;
        uint16_t flags;
        uint32_t offset;
        uint32_t size_compressed;
        std::array<std::uint8_t, 8> checksum;
        uint32_t size_uncompressed;
    };

    struct RMANChunk {
        ChunkID id;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
    };


    struct RMANBundle {
        BundleID id;
        std::vector<RMANChunk> chunks;
    };

    struct RMANLang {
        LangID id;
        std::u8string name;
    };

    struct RMANFile {
        FileID id;
        DirID parent_dir_id;
        uint32_t size;
        std::u8string name;
        uint64_t locale_flags;
        uint8_t unk5;
        uint8_t unk6;
        std::u8string link;
        uint8_t unk8;
        std::vector<ChunkID> chunk_ids = {};
        uint8_t unk10;
        uint8_t params_index;
        uint8_t permissions;
    };

    struct RMANDir {
        DirID id;
        DirID parent_dir_id;
        std::u8string name;
    };

    struct FileChunk : RMANChunk {
        BundleID bundle_id;
        uint32_t compressed_offset;
        uint32_t uncompressed_offset;
    };

    struct FileInfo {
        FileID id;
        uint32_t size;
        std::u8string path;
        std::u8string link;
        std::unordered_set<std::u8string> langs;
        std::vector<FileChunk> chunks;

        void sanitize(std::uint32_t chunkLimit = 16 * 1024 * 1024) const;
    };

    struct RMANManifest {
        uint64_t id;
        std::vector<RMANBundle> bundles;
        std::vector<RMANLang> langs;
        std::vector<RMANFile> files;
        std::vector<RMANDir> dirs;

        static RMANManifest read(std::span<char const> src_data);
        std::vector<FileInfo> list_files() const;
    };
}
