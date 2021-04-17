#include <common/bt_error.hpp>
#include <file/hashlist.hpp>
#include <file/wad.hpp>
#include <zstd.h>
#include <miniz.h>

using namespace file;

struct FileWAD::Reader : IReader {
    Reader(wad::Entry const& info, std::shared_ptr<IReader> source) :
        info_(info), source_(source)
    {}

    std::size_t size() const override final {
        return static_cast<std::size_t>(info_.size_uncompressed);
    }
protected:
    wad::Entry info_;
    std::shared_ptr<IReader> source_;
};

struct FileWAD::ReaderUncompressed final : FileWAD::Reader {
    ReaderUncompressed(wad::Entry const& info, std::shared_ptr<IReader> source) :
        Reader(info, source)
    {}

    std::span<char const> read(std::size_t offset, std::size_t size) override {
        bt_assert(info_.size_uncompressed >= offset + size);
        return source_->read(info_.offset + offset, size);
    }
};

struct FileWAD::ReaderZSTD final : FileWAD::Reader {
    ReaderZSTD(wad::Entry const& info, std::shared_ptr<IReader> source) :
        Reader(info, source), dctx_(ZSTD_createDCtx(), &ZSTD_freeDCtx)
    {
        bt_trace(u8"path hash: {:016X}", info_.path);
        data_ = bt_rethrow(std::unique_ptr<char[]>(new char[static_cast<std::size_t>(info_.size_uncompressed)]));
        auto const result_begin = ZSTD_decompressBegin(dctx_.get());
        bt_trace("zstd error: {}", ZSTD_getErrorName(result_begin));
        bt_assert(!ZSTD_isError(result_begin));
    }

    std::span<char const> read(std::size_t offset, std::size_t size) override {
        bt_assert(info_.size_uncompressed >= offset + size);
        while (pos_uncompressed_ < offset + size) {
            auto const result_src = ZSTD_nextSrcSizeToDecompress(dctx_.get());
            bt_trace("zstd error: {}", ZSTD_getErrorName(result_src));
            bt_assert(!ZSTD_isError(result_src));
            auto const src = source_->read(info_.offset + pos_compressed_, result_src);
            auto const dst = data().subspan(pos_uncompressed_);
            auto const result_dst = ZSTD_decompressContinue(dctx_.get(), dst.data(), dst.size(), src.data(), src.size());
            bt_trace("zstd error: {}", ZSTD_getErrorName(result_dst));
            bt_assert(!ZSTD_isError(result_dst));
            pos_compressed_ += result_src;
            pos_uncompressed_ += result_dst;
        }
        return data().subspan(offset, size);
    }
private:
    std::unique_ptr<char[]> data_ = {};
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> dctx_;
    std::size_t pos_compressed_ = {};
    std::size_t pos_uncompressed_ = {};

    std::span<char> data() noexcept {
        return { data_.get(), static_cast<std::size_t>(info_.size_uncompressed) };
    }
};

struct FileWAD::ReaderZLIB final : FileWAD::Reader {
    ReaderZLIB(wad::Entry const& info, std::shared_ptr<IReader> source) :
        Reader(info, source)
    {
        bt_trace(u8"path hash: {:016X}", info_.path);
        data_ = bt_rethrow(std::unique_ptr<char[]>(new char[static_cast<std::size_t>(info_.size_uncompressed)]));
        auto const result_init = mz_inflateInit2(&dctx_, 16 + MAX_WBITS);
        bt_assert(result_init == MZ_OK);
    }
    ~ReaderZLIB() {
        mz_inflateEnd(&dctx_);
    }

    std::span<char const> read(std::size_t offset, std::size_t size) override {
        constexpr std::size_t const CHUNK_SIZE = 64 * 1024;
        bt_assert(info_.size_uncompressed >= offset + size);
        while (pos_uncompressed_ < offset + size) {
            auto const src_size = std::min(CHUNK_SIZE, info_.size_compressed - pos_compressed_);
            auto const src = source_->read(info_.offset + pos_compressed_, src_size);
            auto const dst = data().subspan(offset + size);
            dctx_.next_in = reinterpret_cast<unsigned char const*>(src.data());
            dctx_.avail_in = static_cast<unsigned int>(src.size());
            dctx_.next_out = reinterpret_cast<unsigned char*>(dst.data());
            dctx_.avail_out = static_cast<unsigned int>(dst.size());
            auto const result_zlib = mz_inflate(&dctx_, MZ_SYNC_FLUSH);
            bt_assert(result_zlib == MZ_OK || result_zlib == MZ_STREAM_END);
            pos_compressed_ += src.size() - dctx_.avail_in;
            pos_uncompressed_ += dst.size() - dctx_.avail_out;
        }
        return data().subspan(offset, size);
    }
private:
    std::unique_ptr<char[]> data_ = {};
    mz_stream dctx_ = {};
    std::size_t pos_compressed_ = {};
    std::size_t pos_uncompressed_ = {};

    std::span<char> data() noexcept {
        return { data_.get(), static_cast<std::size_t>(info_.size_uncompressed) };
    }
};

FileWAD::FileWAD(wad::Entry const&info, std::shared_ptr<IReader> source)
    : info_(info), source_(source)
{}

std::u8string FileWAD::find_name(HashList& hashes) {
    return hashes.find_name_by_hash(info_.path);
}

std::uint64_t FileWAD::find_hash([[maybe_unused]] HashList& hashes) {
    return info_.path;
}

std::u8string FileWAD::find_extension(HashList& hashes) {
    auto ext = hashes.find_extension_by_hash(info_.path);
    if (ext.empty()) {
        if (auto link = get_link(); !link.empty()) {
            ext = hashes.find_extension_by_name(link);
        } else {
            auto const reader = open();
            auto const header_size = std::min(info_.size_uncompressed, 32u);
            auto const header_data = reader->read(0, header_size);
            ext = hashes.find_extension_by_data(info_.path, header_data);
        }
    }
    return ext;
}

std::u8string FileWAD::get_link() {
    if (!link_.empty()) {
        return link_;
    }
    if (info_.type != wad::Entry::Type::FileRedirection) {
        return {};
    }
    auto const src = source_->read(info_.offset, info_.size_uncompressed);
    bt_assert(src.size() > 4);
    std::uint32_t linkSize;
    std::memcpy(&linkSize, src.data(), sizeof(linkSize));
    bt_assert(src.size() >= linkSize + 4);
    link_ = { src.data() + 4, src.data() + 4 + linkSize };
    return link_;
}

std::size_t FileWAD::size() const {
    if (info_.type == wad::Entry::Type::FileRedirection) {
        return 0;
    }
    return static_cast<std::size_t>(info_.size_uncompressed);
}

std::shared_ptr<IReader> FileWAD::open() {
    if (auto result = reader_.lock()) {
        return result;
    } else {
        switch (info_.type) {
        case wad::Entry::Type::FileRedirection:
            bt_error("Links can't be read!");
            break;
        case wad::Entry::Type::Uncompressed:
            reader_ = bt_rethrow(result = std::make_shared<ReaderUncompressed>(info_, source_));
            break;
        case wad::Entry::Type::ZStandardCompressed:
            reader_ = bt_rethrow(result = std::make_shared<ReaderZSTD>(info_, source_));
            break;
        case wad::Entry::Type::ZlibCompressed:
            reader_ = bt_rethrow(result = std::make_shared<ReaderZLIB>(info_, source_));
            break;
        default:
            bt_error("Unknown file type!");
            break;
        }
        return result;
    }
}

IFile::List FileWAD::list_wad(std::shared_ptr<IReader> source) {
    auto result = List{};
    auto wad = wad::EntryList{};
    auto const header_size = wad.read_header_size(source->read(0, sizeof(wad::Header)));
    auto const toc_size = wad.read_toc_size(source->read(0, header_size));
    auto entries = wad.read_entries(source->read(0, toc_size));
    result.reserve(entries.size());
    for (auto const& entry: entries) {
        result.emplace_back(std::make_shared<FileWAD>(entry, source));
    }
    return result;
}
