#pragma once
#include <cstddef>
#include <span>
#include <string_view>

struct Magic {
    std::string_view magic = {};
    std::u8string_view extension = {};
    std::size_t offset = 0;

    template<std::size_t SM, std::size_t SE>
    [[nodiscard]] constexpr Magic(char const(&magic)[SM], char8_t const(&extension)[SE], std::size_t offset = 0) noexcept
        : magic({magic, SM - 1}), extension({extension, SE - 1}), offset(offset)
    {}

    [[nodiscard]] static constexpr std::u8string_view find(std::span<char const> data) noexcept;
};

[[nodiscard]] constexpr std::u8string_view Magic::find(std::span<char const> data) noexcept {
    constexpr Magic const list[] = {
        { "RW\x01\x00", u8".wad" },
        { "RW\x02\x00", u8".wad" },
        { "RW\x03\x00", u8".wad" },
        { "RST", u8".rst" },
        { "\x00\x00\x01\x00", u8".ico" },
        { "\x47\x49\x46\x38\x37\x61", u8".gif" },
        { "\x47\x49\x46\x38\x39\x61", u8".gif" },
        { "\x49\x49\x2A\x00", u8".tif" },
        { "\x49\x49\x00\x2A", u8".tiff" },
        { "\xFF\xD8\xFF\xDB", u8".jpg" },
        { "\xFF\xD8\xFF\xE0", u8".jpg" },
        { "\xFF\xD8\xFF\xEE", u8".jpg" },
        { "\xFF\xD8\xFF\xE1", u8".jpg" },
        { "\x42\x4D", u8".bmp" },
        { "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", u8".png" },
        { "DDS", u8".dds" },
        { "OggS", u8".ogg" },
        { "\x00\x01\x00\x00\x00", u8".ttf" },
        { "\x74\x72\x75\x65\x00", u8".ttf" },
        { "OTTO\x00", u8".otf" },
        { "PROP", u8".bin" },
        { "PTCH", u8".bin" },
        { "BKHD", u8".bnk" },
        { "[ObjectBegin]", u8".sco" },
        { "r3d2Mesh", u8".scb" },
        { "r3d2aims", u8".aimesh" },
        { "r3d2anmd", u8".anm" },
        { "r3d2canm", u8".anm" },
        { "r3d2sklt", u8".skl" },
        { "r3d2blnd", u8".blnd" },
        { "r3d2wght", u8".wgt" },
        { "r3d2", u8".wpk" },
        { "\xC3\x4F\xFD\x22", u8".skl", 4 },
        { "\x33\x22\x11\x00", u8".skn" },
        { "PreLoad", u8".preload" },
        { "\x1BLuaQ\x00\x01\x04\x04", u8".luabin" },
        { "\x1BLuaQ\x00\x01\x04\x08", u8".luabin64" },
        { "OPAM", u8".mob" },
        { "[MaterialBegin]", u8".mat" },
        { "WGEO", u8".wgeo" },
        { "MGEO", u8".mapgeo" },
        { "OEGM", u8".mapgeo" },
        { "NVR\x00", u8".nvr" },
        { "{\r\n", u8".json" },
        { "[\r\n", u8".json" },
        { "<?xml", u8".xml" },
        // There is more of those:
        { "\x00\x00\x0A\x00\x00\x00\x00\x00", u8".tga"},
        { "\x00\x00\x0A\x00\x00\x00\x00\x00", u8".tga"},
        { "\x00\x00\x03\x00\x00\x00\x00\x00", u8".tga"},
        { "\x00\x00\x02\x00\x00\x00\x00\x00", u8".tga"},
        { "\x50\x4B\x03\x04", u8".zip" },
        { "\x52\x61\x72\x21\x1A\x07", u8".raw" },
        { "\x1F\x8B\x08", u8".gz" },
        { "\x75\x73\x74\x61\x72", u8".tar" },
        /*
         * aimesh_ngrid
         * ngrid_overlay
         * rg_overlay
         * info
         * legacydirlistinfo
         * ddf
         * cur
         * usm
        */
    };
    for(auto const& [magic, extension, offset]: list) {
        auto src = std::string_view { data.data() , data.size() };
        if (src.size() >= offset) {
            src = src.substr(offset);
        } else {
            continue;
        }
        if (src.starts_with(magic)) {
            return extension;
        }
    }
    return u8"";
}
