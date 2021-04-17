#include <common/bt_error.hpp>
#include <common/fs.hpp>
#include <common/fltbf.hpp>
#include <file/rman/manifest.hpp>
#include <unordered_map>
#include <cstring>
#include <zstd.h>
#include <limits>

namespace fs = fs;
using namespace rman;
using namespace fltbf;

RMANManifest RMANManifest::read(std::span<char const> data) {
    thread_local auto const dctx = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>(ZSTD_createDCtx(), &ZSTD_freeDCtx);
    RMANHeader header;
    bt_assert(data.size() >= sizeof(RMANHeader));
    std::memcpy(&header, data.data(), sizeof(RMANHeader));
    bt_assert(header.magic == std::array { u8'R', u8'M', u8'A', u8'N', });
    bt_assert(header.offset >= sizeof(RMANHeader));
    bt_assert(data.size() >= header.offset + header.size_compressed);
    auto const src = data.subspan(header.offset, header.size_compressed);
    auto const dst = bt_rethrow(std::unique_ptr<char[]>(new char[header.size_uncompressed]));
    auto zstd_result = ZSTD_decompressDCtx(dctx.get(), dst.get(), header.size_uncompressed, src.data(), src.size());
    bt_assert(!ZSTD_isError(zstd_result));
    auto flatbuffer = Offset { dst.get(), 0, static_cast<int32_t>(header.size_uncompressed) };

    auto body = RMANManifest{};
    auto body_table = flatbuffer.as<Table>();
    for (auto const& bundle_table : body_table[0].as<std::vector<Table>>()) {
        auto &bundle = body.bundles.emplace_back();
        bundle.id = bundle_table[0].as<BundleID>();
        for (auto const& chunk_table : bundle_table[1].as<std::vector<Table>>()) {
            auto &chunk = bundle.chunks.emplace_back();
            chunk.id = chunk_table[0].as<ChunkID>();
            chunk.compressed_size = chunk_table[1].as<int32_t>();
            chunk.uncompressed_size = chunk_table[2].as<int32_t>();
        }
    }
    for (auto const& lang_table : body_table[1].as<std::vector<Table>>()) {
        auto &lang = body.langs.emplace_back();
        lang.id = lang_table[0].as<LangID>();
        lang.name = lang_table[1].as<std::u8string>();
    }
    for (auto const& file_table : body_table[2].as<std::vector<Table>>()) {
        auto &file = body.files.emplace_back();
        file.id = file_table[0].as<FileID>();
        file.parent_dir_id = file_table[1].as<DirID>();
        file.size = file_table[2].as<int32_t>();
        file.name = file_table[3].as<std::u8string>();
        file.locale_flags = file_table[4].as<uint64_t>();
        file.unk5 = file_table[5].as<uint8_t>();                    // ???, unk size
        file.unk6 = file_table[6].as<uint8_t>();                    // ???, unk size
        file.chunk_ids = file_table[7].as<std::vector<ChunkID>>();
        file.unk8 = file_table[8].as<uint8_t>();                    // set to 1 when part of .app
        file.link = file_table[9].as<std::u8string>();
        file.unk10 = file_table[10].as<uint8_t>();                  // ???, unk size
        file.params_index = file_table[11].as<uint8_t>();
        file.permissions = file_table[12].as<uint8_t>();
    }
    for (auto const&  dir_table : body_table[3].as<std::vector<Table>>()) {
        auto &dir = body.dirs.emplace_back();
        dir.id = dir_table[0].as<DirID>();
        dir.parent_dir_id = dir_table[1].as<DirID>();
        dir.name = dir_table[2].as<std::u8string>();
    }
    return body;
}

std::vector<FileInfo> RMANManifest::list_files() const {
    auto const &manifest = *this;
    auto files = std::vector<FileInfo>{};
    auto dir_lookup = std::unordered_map<DirID, RMANDir> {};
    for (auto const& dir: manifest.dirs) {
        dir_lookup[dir.id] = dir;
    }
    auto lang_lookup = std::unordered_map<LangID, std::u8string> {};
    for (auto const& lang: manifest.langs) {
        lang_lookup[lang.id] = lang.name;
    }
    auto chunk_lookup = std::unordered_map<ChunkID, FileChunk> {};
    for (auto const& bundle: manifest.bundles) {
        int32_t compressed_offset = 0;
        for (auto const& chunk: bundle.chunks) {
            chunk_lookup[chunk.id] = FileChunk{chunk, bundle.id, compressed_offset, {}};
            compressed_offset += chunk.compressed_size;
        }
    }
    auto visited = std::unordered_set<DirID>();
    visited.reserve(dir_lookup.size());
    for(auto const& file: manifest.files) {
        bt_trace(u8"FileID: {:016X}", file.id);
        auto& file_info = files.emplace_back();
        file_info.id = file.id;
        file_info.size = file.size;
        file_info.link = file.link;
        auto path = fs::path{file.name};
        auto parent_dir_id = file.parent_dir_id;
        visited.clear();
        while (parent_dir_id != DirID::None) {
            bt_trace(u8"DirID: {}", parent_dir_id);
            bt_assert(visited.find(parent_dir_id) == visited.end());
            visited.insert(parent_dir_id);
            auto const& dir = bt_rethrow(dir_lookup.at(parent_dir_id));
            if (!dir.name.empty()) {
                path = dir.name / path;
                parent_dir_id = dir.parent_dir_id;
            }
        }
        file_info.path = path.generic_u8string();
        for(size_t i = 0; i != 32; i++) {
            if (file.locale_flags & (1ull << i)) {
                bt_trace(u8"LangID: {}", i);
                auto const& lang = bt_rethrow(lang_lookup.at(static_cast<LangID>(i + 1)));
                file_info.langs.insert(lang);
            }
        }
        if (file_info.langs.empty()) {
            file_info.langs.insert(u8"none");
        }
        int32_t uncompressed_offset = 0;
        file_info.chunks.reserve(file.chunk_ids.size());
        for (auto chunk_id: file.chunk_ids) {
            bt_trace(u8"ChunkID: {:016X}", chunk_id);
            auto& chunk = file_info.chunks.emplace_back();
            bt_rethrow(chunk = chunk_lookup.at(chunk_id));
            chunk.uncompressed_offset = uncompressed_offset;
            uncompressed_offset += chunk.uncompressed_size;
        }
    }
    return files;
}

void FileInfo::sanitize(std::int32_t chunkLimit) const {
    auto const& file = *this;
    bt_trace(u8"File id: {:016X}, name: {}", file.id, file.path);
    bt_assert(file.id != FileID::None);
    bt_assert(file.link.empty());
    bt_assert(!file.path.empty());
    bt_assert(file.path.size() < 256);
    auto path = fs::path(file.path, fs::path::generic_format);
    auto path_normalized = path.lexically_normal().generic_u8string();
    bt_assert(file.path == path_normalized);
    bt_assert(!path.is_absolute());
    for (auto const& component: path) {
        auto path_component = component.generic_u8string();
        bt_assert(!path_component.empty());
        bt_assert(path_component != u8".." && path_component != u8".");
    }
    bt_assert(file.size > 0);
    bt_assert(file.size <= (INT32_MAX - chunkLimit));
    auto const max_compressed = ZSTD_COMPRESSBOUND(chunkLimit);
    auto next_min_uncompressed_offset = int32_t{0};
    for (auto const& chunk: file.chunks) {
        bt_trace(u8"Chunk id: {:016X}", chunk.id);
        bt_assert(chunk.id != ChunkID::None);
        bt_assert(chunk.compressed_size >= 4);
        bt_assert(chunk.compressed_size <= max_compressed);
        bt_assert(chunk.compressed_offset >= 0);
        bt_assert(chunk.bundle_id != BundleID::None);
        bt_assert(chunk.uncompressed_size > 0);
        bt_assert(chunk.uncompressed_size <= chunkLimit);
        bt_assert(chunk.uncompressed_offset >= next_min_uncompressed_offset);
        bt_assert(chunk.uncompressed_offset + chunk.uncompressed_size <= file.size);
        next_min_uncompressed_offset = chunk.uncompressed_offset + chunk.uncompressed_size;
    }
}

