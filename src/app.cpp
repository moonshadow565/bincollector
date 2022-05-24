#include <common/bt_error.hpp>
#include <common/fs.hpp>
#include <common/mmap.hpp>
#include <common/xxhash64.hpp>
#include <charconv>
#include <iostream>
#include "app.hpp"
#include "argparse.hpp"

static std::set<std::u8string> parse_list(std::string const& value) {
    std::set<std::u8string> result = {};
    size_t next = 0;
    size_t end = value.size();
    auto skip_whitespace = [&](size_t cur) -> size_t {
        while (cur != end && (value[cur] == ' ' || value[cur] == ',')) {
            cur++;
        }
        return cur;
    };
    auto get_next = [&](size_t cur) -> size_t {
        while (cur != end && value[cur] != ',') {
            cur++;
        }
        return cur;
    };
    while(next != end) {
        auto start = skip_whitespace(next);
        next = get_next(start);
        auto size = next - start;
        if (size > 0) {
            result.insert(to_lower(from_std_string(value.substr(start, size))));
        }
    }
    return result;
}

static std::set<std::uint64_t> parse_hash_list(std::string const& value) {
    auto strings = parse_list(value);
    auto results = std::set<std::uint64_t>{};
    for (auto const& str: strings) {
        if (str.starts_with(u8"0x")) {
            auto result = std::uint64_t{};
            auto const start = reinterpret_cast<char const*>(str.data()) + 2;
            auto const end = reinterpret_cast<char const*>(str.data()) + str.size();
            auto err_ptr = std::from_chars(start, end, result, 16);
            bt_trace(u8"str: {}", str);
            bt_assert(err_ptr.ec == std::errc{} && err_ptr.ptr == end);
        } else {
            results.insert(XXH64(str));
        }
    }
    return results;
}

static std::u8string get_version(std::span<char const> data) noexcept {
    auto databeg = reinterpret_cast<char16_t const*>(data.data());
    auto dataend = databeg + (data.size() / 2);
    for (;;) {
        auto const pat = std::u16string_view{ u"\u0001ProductVersion" };
        auto const vtag = std::search(databeg, dataend,
            std::boyer_moore_searcher(pat.cbegin(), pat.cend()));
        if (vtag == dataend) {
            return {};
        }
        auto const vbeg = vtag + pat.size() + 1;
        auto const vend = std::find(vbeg, dataend, u'\u0000');
        auto version = std::u8string(vend - vbeg, '\0');
        std::transform(vbeg, vend, version.begin(), [](char16_t c) { return static_cast<char8_t>(c); });

        constexpr auto isBadChar = [](char8_t c) { return c == u8'.' || (c >= u8'0' && c <= u8'9'); };
        if (std::find_if_not(version.cbegin(), version.cend(), isBadChar) != version.cend()) {
            databeg = vend;
            continue;
        };

        return version;
    }
    return {};
}

App::App(fs::path src_dir) : src_dir(src_dir) {}

void App::load_hashes() {
    if (!action.has_hashes) {
        return;
    }
    if (hash_path_names.empty()) {
        if (auto p = src_dir / u8"hashes" / u8"hashes.game.txt"; fs::exists(p)) {
            hash_path_names = p.generic_u8string();
        } else if (auto p = fs::path(u8"./") / u8"hashes.game.txt"; fs::exists(p)) {
            hash_path_names = p.generic_u8string();
        } else {
            hash_path_names = p.generic_u8string();
        }
    }
    if (hash_path_extensions.empty()) {
        if (auto p = src_dir / u8"hashes" / u8"hashes.game.ext.txt"; fs::exists(p)) {
            hash_path_extensions = p.generic_u8string();
        } else if (auto p = fs::path(u8"./") / u8"hashes.game.ext.txt"; fs::exists(p)) {
            hash_path_extensions = p.generic_u8string();
        } else {
            hash_path_extensions = p.generic_u8string();
        }
    }
    if (fs::exists(hash_path_names)) {
        hashlist.read_names_list(hash_path_names);
    }
    if (fs::exists(hash_path_extensions)) {
        hashlist.read_extensions_list(hash_path_extensions);
    }
}

void App::save_hashes() {
    if (!action.has_hashes) {
        return;
    }
    hashlist.write_names_list(hash_path_names);
    hashlist.write_extensions_list(hash_path_extensions);
}

void App::parse_args(int argc, char** argv) {
    std::string valid_actions = "";
    for (auto const& action: ACTIONS) {
        valid_actions += action.long_name;
        valid_actions += ", ";
    }
    argparse::ArgumentParser program("bincollector");
    program.add_argument("action")
            .help("action: " + valid_actions)
            .required()
            .action([](std::string const& value){
                for (auto const& action: ACTIONS) {
                    if (action.long_name == value || (action.short_name && action.short_name == value)) {
                        return action;
                    }
                }
                throw std::runtime_error("Unknown action!");
            });
    program.add_argument("manifest")
            .help(".releasemanifest / .manifest / .wad / folder ")
            .required();
    program.add_argument("cdn")
            .help("cdn for manifest and releasemanifest (for raw wad files this is root of game folder)")
            .default_value(std::string{});
    program.add_argument("-r", "--remote")
            .help("Input: remote http mirror to fetch files from(only works for .manifest files).")
            .default_value(std::string{});
    program.add_argument("-o", "--output")
            .help("Output directory for extract")
            .default_value(std::string{"."});
    program.add_argument("-l", "--lang")
            .help("Filter: language(none for international files).")
            .default_value(std::string{});
    program.add_argument("-p", "--path")
            .help("Filter: paths or path hashes.")
            .default_value(std::string{});
    program.add_argument("-e", "--ext")
            .help("Filter: extensions with . (dot)")
            .default_value(std::string{});
    program.add_argument("--hashes-names")
            .help("File: Hash list for names")
            .default_value(std::string{});
    program.add_argument("--hashes-exts")
            .help("File: Hash list for extensions")
            .default_value(std::string{});
    program.add_argument("--skip-root")
            .help("Skip processing files in root.")
            .default_value(false)
            .implicit_value(true);
    program.add_argument("-w", "--show-wads")
        .help("Show .wad files in dump")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("-d", "--max-depth")
        .help("Max depth to recurse into.")
        .default_value(int(0))
        .action([](std::string value) {
            return std::stoi(value);
        });

    program.parse_args(argc, argv);
    action = program.get<Action>("action");
    manifest = from_std_string(program.get<std::string>("manifest"));
    cdn = from_std_string(program.get<std::string>("cdn"));
    remote = from_std_string(program.get<std::string>("--remote"));
    output = from_std_string(program.get<std::string>("--output"));
    langs = parse_list(program.get<std::string>("--lang"));
    names = parse_hash_list(program.get<std::string>("--path"));
    extensions = parse_list(program.get<std::string>("--ext"));
    hash_path_names = from_std_string(program.get<std::string>("--hashes-names"));
    hash_path_extensions = from_std_string(program.get<std::string>("--hashes-exts"));
    max_depth = program.get<int>("--max-depth");
    show_wads = program.get<bool>("--show-wads");
    skip_root = program.get<bool>("--skip-root");
}

void App::run() {
    auto manager = file::IManager::make(manifest, cdn, remote, langs);
    return (this->*action.handler)(manager, 1);
}

void App::list_manager(std::shared_ptr<file::IManager> manager, int depth) {
    for (auto const& entry: manager->list()) {
        bt_trace(u8"location: {}", entry->location()->print(u8";"));
        auto hash = entry->find_hash(hashlist);
        if (!names.empty() && !names.contains(hash)) {
            continue;
        }
        std::shared_ptr<file::IReader> reader = nullptr;
        if (entry->is_wad()) {
            if (!max_depth || depth < max_depth) {
                reader = entry->open();
                auto wad = std::make_shared<file::ManagerWAD>(entry);
                list_manager(wad, depth + 1);
            }
            if (!show_wads) {
                continue;
            }
        }
        if (depth == 1 && skip_root) {
            continue;
        }
        auto ext = entry->find_extension(hashlist);
        if (!extensions.empty() && !extensions.contains(ext)) {
            continue;
        }
        auto name = entry->find_name(hashlist);
        auto id = entry->id();
        auto size = entry->size();
        fmt_print(std::cout, u8"{:016x},{},{},{},{}\n", hash, ext, name, id, size);
    }
}

void App::extract_manager(std::shared_ptr<file::IManager> manager, int depth) {
    for (auto const& entry: manager->list()) {
        bt_trace(u8"location: {}", entry->location()->print(u8";"));
        auto hash = entry->find_hash(hashlist);
        if (!names.empty() && !names.contains(hash)) {
            continue;
        }
        std::shared_ptr<file::IReader> reader = nullptr;
        if (entry->is_wad()) {
            if (!max_depth || depth < max_depth) {
                reader = entry->open();
                auto wad = std::make_shared<file::ManagerWAD>(entry);
                extract_manager(wad, depth + 1);
            }
            if (!show_wads) {
                continue;
            }
        }
        if (depth == 1 && skip_root) {
            continue;
        }
        auto ext = entry->find_extension(hashlist);
        if (!extensions.empty() && !extensions.contains(ext)) {
            continue;
        }
        auto link = entry->get_link();
        if (!link.empty()) {
            continue;
        }
        auto name = entry->find_name(hashlist);
        auto out_name = name;
        if (out_name.empty() || out_name.size() > 127) {
            out_name = fmt::format(u8"{:016x}{}", hash, ext);
        }
        entry->extract_to(fs::path(output) / out_name);
    }
}

void App::index_manager(std::shared_ptr<file::IManager> manager, int depth) {
    for (auto const& entry: manager->list()) {
        bt_trace(u8"location: {}", entry->location()->print(u8";"));
        auto hash = entry->find_hash(hashlist);
        if (!names.empty() && !names.contains(hash)) {
            continue;
        }
        std::shared_ptr<file::IReader> reader = nullptr;
        if (entry->is_wad()) {
            if (!max_depth || depth < max_depth) {
                reader = entry->open();
                auto wad = std::make_shared<file::ManagerWAD>(entry);
                index_manager(wad, depth + 1);
            }
            if (!show_wads) {
                continue;
            }
        }
        if (depth == 1 && skip_root) {
            continue;
        }
        auto ext = entry->find_extension(hashlist);
        if (!extensions.empty() && !extensions.contains(ext)) {
            continue;
        }
        auto link = entry->get_link();
        if (!link.empty()) {
            continue;
        }
        auto name = entry->find_name(hashlist);
        auto id = entry->id();
        auto size = entry->size();
        fmt_print(std::cout, u8"{:016x},{},{},{},{}\n", hash, ext, name, id, size);
        auto out_name = fs::path(output) / id;
        if (!fs::exists(out_name)) {
            entry->extract_to(fs::path(output) / id);
        }
    }
}

void App::exe_ver(std::shared_ptr<file::IManager> manager, int depth) {
    for (auto const& entry: manager->list()) {
        bt_trace(u8"location: {}", entry->location()->print(u8";"));
        auto ext = entry->find_extension(hashlist);
        if (ext != u8".exe") {
            continue;
        }
        auto hash = entry->find_hash(hashlist);
        if (!names.empty() && !names.contains(hash)) {
            continue;
        }
        auto name = entry->find_name(hashlist);
        auto reader = entry->open();
        auto data = reader->read();
        auto version = get_version(data);
        fmt_print(std::cout, u8"{},{}\n", name, version);
    }
}

void App::checksum_manager(std::shared_ptr<file::IManager> manager, int depth) {
    for (auto const& entry: manager->list()) {
        auto location = entry->location()->print(u8";");
        bt_trace(u8"location: {}", location);
        auto hash = entry->find_hash(hashlist);
        if (!names.empty() && !names.contains(hash)) {
            continue;
        }
        std::shared_ptr<file::IReader> reader = nullptr;
        if (entry->is_wad()) {
            if (!max_depth || depth < max_depth) {
                reader = entry->open();
                auto wad = std::make_shared<file::ManagerWAD>(entry);
                checksum_manager(wad, depth + 1);
            }
            if (!show_wads) {
                continue;
            }
        }
        if (depth == 1 && skip_root) {
            continue;
        }
        auto ext = entry->find_extension(hashlist);
        if (!extensions.empty() && !extensions.contains(ext)) {
            continue;
        }
        auto checksums = entry->checksums().print();
        auto name = entry->find_name(hashlist);
        fmt_print(std::cout,
                  u8"{},{:016x}{},{},{}\n",
                  checksums, hash, ext, name, location);
    }
}
