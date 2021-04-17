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

std::vector<Entry> EntryList::read_entries(std::span<char const> data) {
    bt_assert(data.size() >= header.entry_offset + header.entry_count * header.entry_size);
    data = data.subspan(header.entry_offset);
    auto entries = std::vector<Entry>{};
    entries.resize(header.entry_count);
    for (std::size_t i = 0; i != header.entry_count; ++i) {
        std::memcpy(&entries[i], data.data() + i * header.entry_size, sizeof(Entry));
    }
    return entries;
}
