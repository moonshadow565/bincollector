#include <common/bt_error.hpp>
#include <common/mmap.hpp>
#include <file/hashlist.hpp>
#include <file/rman.hpp>
#include <file/rman/manifest.hpp>
#include <zstd.h>

using namespace file;

struct FileRMAN::Reader final : IReader {
    Reader(rman::FileInfo const& info, fs::path const& base)
        : info_(info), base_(base)
    {
        bt_trace(u8"path: {}", info_.path);
        data_ = bt_rethrow(std::unique_ptr<char[]>(new char[static_cast<std::size_t>(info_.size)]));
        maped_.reserve(info_.chunks.size());
    }

    std::size_t size() const override {
        return static_cast<std::size_t>(info_.size);
    }

    std::span<char const> read(std::size_t offset, std::size_t size) override {
        bt_trace(u8"path: {}", info_.path);
        bt_assert(data().size() >= offset + size);
        bt_assert(offset + size == 0 || data_);
        // Get chunks inside specific range, guaranteed to succeed if assert passes
        auto const chunks = chunks_in_range(static_cast<std::int32_t>(offset), static_cast<std::int32_t>(size));

        // Ranges are sorted in order by: BundleID, ChunkID, UncompressedOffset
        auto bundle = MMap<char const>{};
        auto last_bundle_id = rman::BundleID::None;

        // Cache decompression context in this thread
        thread_local auto const dctx = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>(ZSTD_createDCtx(), &ZSTD_freeDCtx);
        for (auto i = chunks.begin(); i != chunks.end();) {
            // Skip any offsets that are already uncompressed
            if (maped_.contains(i->uncompressed_offset)) {
                last_bundle_id = rman::BundleID::None;
                ++i;
                continue;
            }
            bt_trace(u8"bundle: {:016X}", i->bundle_id);

            // If we are in new bundle we need to open it
            if (i->bundle_id != last_bundle_id) {
                auto const path = base_ / u8"bundles" / fmt::format(u8"{:016X}.bundle", i->bundle_id);
                bt_trace(u8"base: {}", base_.generic_u8string());
                bt_rethrow(bundle.open(path).unwrap());
                last_bundle_id = i->bundle_id;
            }

            // Uncompress chunk in specific offset
            auto const dst = data().subspan(static_cast<std::size_t>(i->uncompressed_offset),
                                            static_cast<std::size_t>(i->uncompressed_size));
            auto const src = bundle.span().subspan(static_cast<std::size_t>(i->compressed_offset),
                                                   static_cast<std::size_t>(i->compressed_size));
            auto const result = ZSTD_decompressDCtx(dctx.get(), dst.data(), dst.size(), src.data(), src.size());
            bt_trace("zstd error: {}", ZSTD_getErrorName(result));
            bt_assert(!ZSTD_isError(result));
            maped_.insert(i->uncompressed_offset);

            // Find any following chunks that use same data
            auto const last_chunk_id = i->id;
            for (++i; i != chunks.end() && i->id == last_chunk_id; ++i) {
                auto const cpy = data().subspan(static_cast<std::size_t>(i->uncompressed_offset),
                                                static_cast<std::size_t>(i->uncompressed_size));
                std::memcpy(cpy.data(), dst.data(), dst.size());
                maped_.insert(i->uncompressed_offset);
            }
        }
        return data().subspan(offset, size);
    }
private:
    rman::FileInfo info_;
    fs::path base_;
    std::unique_ptr<char[]> data_ = {};
    std::unordered_set<std::int32_t> maped_;

    std::span<char> data() noexcept {
        return { data_.get(), static_cast<std::size_t>(info_.size) };
    }

    std::vector<rman::FileChunk> chunks_in_range(std::int32_t offset, std::int32_t size) const noexcept {
        auto const compare_offset = [](auto const& lhs, auto const& rhs) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(lhs)>, std::int32_t>) {
                return lhs < rhs.uncompressed_offset;
            } else {
                return lhs.uncompressed_offset < rhs;
            }
        };
        auto const compare_id = [] (auto const& lhs, auto const& rhs) {
            return std::tie(lhs.bundle_id, lhs.id, lhs.uncompressed_offset)
                    < std::tie(rhs.bundle_id, rhs.id, rhs.uncompressed_offset);
        };
        auto const start = std::lower_bound(info_.chunks.begin(), info_.chunks.end(), offset, compare_offset);
        auto const end = std::lower_bound(start, info_.chunks.end(), offset + size, compare_offset);
        auto ranges = std::vector<rman::FileChunk> { start, end };
        std::sort(ranges.begin(), ranges.end(), compare_id);
        return ranges;
    }
};

FileRMAN::FileRMAN(rman::FileInfo const&info, fs::path const &base)
    : info_(info), base_(base)
{}

std::u8string FileRMAN::find_name([[maybe_unused]] HashList& hashes) {
    return info_.path;
}

std::uint64_t FileRMAN::find_hash(HashList& hashes) {
    return hashes.find_hash_by_name(info_.path);
}

std::u8string FileRMAN::find_extension(HashList& hashes) {
    auto ext = hashes.find_extension_by_name(info_.path);
    if (ext.empty()) {
        if (auto link = get_link(); !link.empty()) {
            ext = hashes.find_extension_by_name(link);
        }
    }
    return ext;
}

std::u8string FileRMAN::get_link() {
    return info_.link;
}

std::size_t FileRMAN::size() const {
    if (!info_.link.empty()) {
        return 0;
    }
    return static_cast<std::size_t>(info_.size);
}

std::shared_ptr<IReader> FileRMAN::open() {
    if (auto result = reader_.lock()) {
        return result;
    } else {
        bt_assert(info_.link.empty());
        reader_ = (result = std::make_shared<Reader>(info_, base_));
        return result;
    }
}

IFile::List FileRMAN::list_manifest(rman::RMANManifest const& manifest, fs::path const& cdn) {
    auto result = List{};
    auto entries = manifest.list_files();
    result.reserve(entries.size());
    for (auto const& entry: entries) {
        // FIXME: do we want to filter this here
        if (!entry.langs.contains(u8"none") && !entry.langs.contains(u8"en_us")) {
            continue;
        }
        result.emplace_back(std::make_shared<FileRMAN>(entry, cdn));
    }
    return result;
}
