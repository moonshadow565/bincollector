#include <common/bt_error.hpp>
#include <file/wad/wad.hpp>
#include <cstring>
#include <cstdlib>

using namespace wad;

std::size_t EntryList::read_header_size(std::span<char const> src) {
    bt_assert(src.size() >= sizeof(Header));
    std::memcpy(&header, src.data(), sizeof(Header));
    bt_assert(header.magic == std::array { u8'R', u8'W' });
    switch (header.versionMajor) {
    case 1:
        return sizeof(HeaderV1);
    case 2:
        return sizeof(HeaderV2);
    case 3:
        return sizeof(HeaderV3);
    default:
        bt_error("Unsuported wad version!");
    }
}

std::size_t EntryList::read_toc_size(std::span<char const> src) {
    bt_trace(u8"Version: {}.{}", header.versionMajor, header.versionMinor);
    switch (header.versionMajor) {
    case 1: {
        HeaderV1 headerV1;
        bt_assert(src.size() >= sizeof(headerV1));
        std::memcpy(&headerV1, src.data(), sizeof(headerV1));
        this->header = headerV1;
    } break;
    case 2:{
        HeaderV2 headerV2;
        bt_assert(src.size() >= sizeof(headerV2));
        std::memcpy(&headerV2, src.data(), sizeof(headerV2));
        this->header = headerV2;
    } break;
    case 3:{
        HeaderV3 headerV3;
        bt_assert(src.size() >= sizeof(headerV3));
        std::memcpy(&headerV3, src.data(), sizeof(headerV3));
        this->header = headerV3;
    } break;
    default:
        bt_error("Unsuported wad version!");
    }
    return header.entry_offset + header.entry_count * header.entry_size;
}

std::vector<EntryInfo> EntryList::read_entries(std::span<char const> data) const {
    bt_assert(data.size() >= header.entry_offset + header.entry_count * header.entry_size);
    data = data.subspan(header.entry_offset);
    auto results = std::vector<EntryInfo>{};
    results.reserve(header.entry_count);
    switch (header.versionMajor) {
    case 1:
    case 2:{
        bt_assert(header.entry_size >= sizeof(Entry));
        for (std::size_t i = 0; i != header.entry_count; ++i) {
            Entry entry;
            std::memcpy(&entry, data.data() + i * header.entry_size, sizeof(Entry));
            results.emplace_back(entry);
        }
    } break;
    case 3:{
        bt_assert(header.entry_size >= sizeof(EntryV3));
        for (std::size_t i = 0; i != header.entry_count; ++i) {
            EntryV3 entry;
            std::memcpy(&entry, data.data() + i * header.entry_size, sizeof(EntryV3));
            std::uint64_t id;
            std::memcpy(&id, &entry.checksum, sizeof(std::uint64_t));
            results.emplace_back(entry, id);
        }
    } break;
    default:
        bt_error("Unsuported wad version!");
    }
    return results;
}
