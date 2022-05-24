#include <zstd.h>

#include <common/bt_error.hpp>
#include <common/mmap.hpp>
#include <file/hashlist.hpp>
#include <file/raw.hpp>
#include <file/rman.hpp>
#include <file/rman/manifest.hpp>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <curl/curl.h>

using namespace file;

struct file::CacheRMAN final  {
    CacheRMAN(fs::path cdn, std::u8string remote) : cdn_(std::move(cdn)), remote_(std::move(remote)) {
        bt_assert(!cdn_.empty());

        auto cdn_str = cdn_.generic_u8string();
        std::transform(cdn_str.begin(), cdn_str.end(), cdn_str.begin(), ::tolower);
        while (cdn_str.ends_with(u8'/') || cdn_str.ends_with(u8'\\')) cdn_str.pop_back();

        if (cdn_str.ends_with(u8"chunks")) {
            is_chunking_ = true;
        } else if (cdn_str.ends_with(u8"bundles")) {
            is_chunking_ = false;
        } else {
            cdn_ /= u8"bundles";
        }

        if (!remote_.empty()) {
            bt_rethrow(fs::create_directories(cdn_));
            bt_assert(curl_ = curl_easy_init());
            bt_assert(curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0) == CURLE_OK);
            bt_assert(curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L) == CURLE_OK);
            curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
            curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &write_remote_bundle);
        }
    }

    std::span<char const> open_chunk(rman::FileChunk const& chunk) {
        // If we have the chunk already in our last use cache, use it
        if (remote_chunk_id == chunk.id) return remote_chunk_buffer;
        if (local_chunk_id == chunk.id) return local_chunk_file.span();

        // Try to open local chunk cache
        if (is_chunking_) {
            auto local_chunk_path = cdn_ / fmt::format(u8"{:016X}.chunk", chunk.id);
            if (fs::exists(local_chunk_path)) {
                local_chunk_id = {};
                bt_rethrow(local_chunk_file.open(local_chunk_path).unwrap());
                local_chunk_id = chunk.id;
                return local_chunk_file.span();
            }
            bt_assert(curl_ && "Local chunk missing and no remote to fallback to!");
        }
        auto bundle = open_bundle(chunk);
        bt_assert(chunk.compressed_size + chunk.compressed_offset <= bundle.size());
        remote_chunk_id = {};
        remote_chunk_buffer.clear();
        remote_chunk_buffer.resize(chunk.uncompressed_size);
        auto result = ZSTD_decompress(remote_chunk_buffer.data(), remote_chunk_buffer.size(),
                                      bundle.data() + chunk.compressed_offset, chunk.compressed_size);
        bt_trace("zstd error: {}", ZSTD_getErrorName(result));
        bt_assert(!ZSTD_isError(result));
        bt_assert(result == chunk.uncompressed_size);
        remote_chunk_id = chunk.id;
        return remote_chunk_buffer;
    }

    std::span<char const> open_bundle(rman::FileChunk const& chunk) {
        // If we already have bundle in our last use cache, use it
        if (remote_bundle_id == chunk.bundle_id) return remote_bundle_buffer;
        if (local_bundle_id == chunk.bundle_id) return local_bundle_file.span();

        // Try to open local bundle
        auto local_bundle_path = cdn_ / fmt::format(u8"{:016X}.bundle", chunk.bundle_id);
        if (!is_chunking_) {
            if (fs::exists(local_bundle_path)) {
                local_bundle_id = {};
                bt_rethrow(local_bundle_file.open(local_bundle_path).unwrap());
                local_bundle_id = chunk.bundle_id;
                return local_bundle_file.span();
            }
        }

        // Fetch remote bundle buffer
        bt_assert(curl_ && "Local bundle missing and no remote to fallback to!");
        remote_bundle_id = {};
        remote_bundle_buffer.clear();
        auto remote_path = remote_ + fmt::format(u8"/bundles/{:016X}.bundle", chunk.bundle_id);
        curl_easy_setopt(curl_, CURLOPT_URL, (char const*)(remote_path.c_str()));
        bt_assert(curl_easy_perform(curl_) == CURLE_OK);

        auto rbun = rman::RBUNBundle::read(remote_bundle_buffer);
        bt_assert(rbun.id == chunk.bundle_id);
        remote_bundle_id = chunk.bundle_id;

        // Write remote bundle or chunks to local filesystem
        if (!is_chunking_) {
            auto out = MMap<char>{};
            bt_rethrow(out.create(local_bundle_path, remote_bundle_buffer.size()).unwrap());
            std::memcpy(out.data(), remote_bundle_buffer.data(), remote_bundle_buffer.size());
        } else {
            for (size_t offset = 0; auto const& chunk: rbun.chunks) {
                auto local_chunk_path = cdn_ / fmt::format(u8"{:016X}.chunk", chunk.id);
                if (!fs::exists(local_chunk_path)) {
                    bt_assert(chunk.compressed_size + offset <= remote_bundle_buffer.size());
                    remote_chunk_id = {};
                    remote_chunk_buffer.clear();
                    remote_chunk_buffer.resize(chunk.uncompressed_size);
                    auto result = ZSTD_decompress(remote_chunk_buffer.data(), remote_chunk_buffer.size(),
                                                  remote_bundle_buffer.data() + offset, chunk.compressed_size);
                    bt_trace("zstd error: {}", ZSTD_getErrorName(result));
                    bt_assert(!ZSTD_isError(result));
                    bt_assert(result == chunk.uncompressed_size);
                    remote_chunk_id = chunk.id;
                    auto out = MMap<char>{};
                    bt_rethrow(out.create(local_chunk_path, chunk.uncompressed_size).unwrap());
                    std::memcpy(out.data(), remote_chunk_buffer.data(), remote_chunk_buffer.size());
                }
                offset += chunk.compressed_size;
            }
        }

        return remote_bundle_buffer;
    }


private:
    fs::path cdn_;
    std::u8string remote_;
    bool is_chunking_ = false;
    void* curl_ = nullptr;

    rman::BundleID remote_bundle_id = {};
    std::vector<char> remote_bundle_buffer = {};

    rman::ChunkID remote_chunk_id = {};
    std::vector<char> remote_chunk_buffer = {};

    rman::BundleID local_bundle_id = {};
    MMap<char const> local_bundle_file = {};

    rman::ChunkID local_chunk_id = {};
    MMap<char const> local_chunk_file = {};

    static size_t write_remote_bundle(char const* p, size_t s, size_t n, CacheRMAN *o) noexcept {
        s *= n;
        o->remote_bundle_buffer.insert(o->remote_bundle_buffer.end(), p, p + s);
        return s;
    }
};

struct FileRMAN::Reader final : IReader {
    Reader(rman::FileInfo const& info, std::shared_ptr<CacheRMAN> cache)
        : info_(info), cache_(cache)
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
        // Ranges are sorted in order by: BundleID, ChunkID, UncompressedOffset
        auto const chunks = chunks_in_range(static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(size));

        auto data = this->data();
        for (auto i = std::span<rman::FileChunk const>(chunks); !i.empty();) {
            auto const& cur = i.front();
            bt_trace(u8"bundle: {:016X}", cur.bundle_id);
            bt_trace(u8"chunk: {:016X}", cur.id);

            auto src = cache_->open_chunk(cur);
            auto dst = data.subspan(cur.uncompressed_offset, cur.uncompressed_size);

            bt_assert(src.size() == cur.uncompressed_size);

            while (!i.empty() && i.front().id == cur.id) {
                auto const& cur = i.front();
                auto const dst = data.subspan(cur.uncompressed_offset, cur.uncompressed_size);
                std::memcpy(dst.data(), src.data(), src.size());
                maped_.insert(cur.uncompressed_offset);
                i = i.subspan(1);
            }
        }

        return data.subspan(offset, size);
    }
private:
    rman::FileInfo info_;
    std::shared_ptr<CacheRMAN> cache_;
    std::unique_ptr<char[]> data_ = {};
    std::unordered_set<std::uint32_t> maped_;

    std::span<char> data() noexcept {
        return { data_.get(), static_cast<std::size_t>(info_.size) };
    }

    std::vector<rman::FileChunk> chunks_in_range(std::uint32_t offset, std::uint32_t size) const noexcept {
        auto const compare_offset = [](auto const& lhs, auto const& rhs) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(lhs)>, std::uint32_t>) {
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
        std::remove_if(ranges.begin(), ranges.end(), [this] (rman::FileChunk const& chunk) {
            return maped_.contains(chunk.uncompressed_offset);
        });
        std::sort(ranges.begin(), ranges.end(), compare_id);
        return ranges;
    }
};

FileRMAN::FileRMAN(rman::FileInfo const&info,
                   std::shared_ptr<CacheRMAN> cache,
                   std::shared_ptr<Location> source_location)
    : info_(info)
    , cache_(cache)
    , location_(std::make_shared<Location>(source_location, info_.path))
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

std::u8string FileRMAN::id() const {
    if (!info_.link.empty()) {
        return {};
    }
    return fmt::format(u8"{:016x}.fid", info_.id);
}

std::shared_ptr<Location> FileRMAN::location() const {
    return location_;
}

std::shared_ptr<IReader> FileRMAN::open() {
    if (auto result = reader_.lock()) {
        return result;
    } else {
        bt_assert(info_.link.empty());
        reader_ = (result = std::make_shared<Reader>(info_, cache_));
        return result;
    }
}

bool FileRMAN::is_wad() {
    if (!info_.link.empty()) {
        return false;
    }
    auto const& name = info_.path;
    return name.ends_with(u8".wad") || name.ends_with(u8".client") || name.ends_with(u8".mobile");
}

ManagerRMAN::ManagerRMAN(std::shared_ptr<IReader> source,
                         fs::path cdn,
                         std::u8string remote,
                         std::set<std::u8string> const& langs,
                         std::shared_ptr<Location> source_location)
    : cache_(std::make_shared<CacheRMAN>(cdn, remote))
    , location_(std::make_shared<Location>(source_location))
{
    auto manifest = rman::RMANManifest::read(source->read());
    location_->path = fmt::format(u8"{:016x}.manifest", manifest.id);
    files_ = manifest.list_files();
    if (!langs.empty()) {
        std::erase_if(files_, [&langs] (rman::FileInfo const& info) -> bool {
            for (auto const& lang: langs) {
                if (info.langs.contains(lang)) {
                    return false;
                }
            }
            return true;
        });
    }
}

std::vector<std::shared_ptr<IFile>> ManagerRMAN::list() {
    auto result = std::vector<std::shared_ptr<IFile>>{};
    result.reserve(files_.size());
    for (auto const& entry: files_) {
        result.emplace_back(std::make_shared<FileRMAN>(entry, cache_, location_));
    }
    return result;
}
