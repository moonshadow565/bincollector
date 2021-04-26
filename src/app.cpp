#include <common/bt_error.hpp>
#include <common/fs.hpp>
#include <common/mmap.hpp>
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
        while (cur != end && (value[cur] != ' ' && value[cur] != ',')) {
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

App::App(fs::path src_dir) : src_dir(src_dir) {}

void App::load_hashes() {
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
    hashlist.write_names_list(hash_path_names);
    hashlist.write_extensions_list(hash_path_extensions);
}

void App::parse_args(int argc, char** argv) {
    argparse::ArgumentParser program("bincollector");
    program.add_argument("action")
            .help("action: list, extract, index ")
            .required()
            .action([](std::string const& value){
                if (value == "list" || value == "ls") {
                    return Action::List;
                }
                if (value == "extract" || value == "ex") {
                    return Action::Extract;
                }
                if (value == "index" || value == "index") {
                    return Action::Index;
                }
                throw std::runtime_error("Unknown action!");
            });
    program.add_argument("manifest")
            .help(".releasemanifest / .manifest / .wad / folder ")
            .required();
    program.add_argument("cdn")
            .help("cdn for manifest and releasemanifest")
            .default_value(std::string{});
    program.add_argument("-o", "--output")
            .help("Output directory for extract")
            .default_value(std::string{"."});
    program.add_argument("-l", "--lang")
            .help("Filter: language(none for international files).")
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
    program.add_argument("--skip-wad")
            .help("Skip .wad processing")
            .default_value(false)
            .implicit_value(true);

    program.parse_args(argc, argv);
    action = program.get<Action>("action");
    manifest = from_std_string(program.get<std::string>("manifest"));
    cdn = from_std_string(program.get<std::string>("cdn"));
    output = from_std_string(program.get<std::string>("--output"));
    langs = parse_list(program.get<std::string>("--lang"));
    extensions = parse_list(program.get<std::string>("--ext"));
    hash_path_names = from_std_string(program.get<std::string>("--hashes-names"));
    hash_path_extensions = from_std_string(program.get<std::string>("--hashes-exts"));
    skip_wad = program.get<bool>("--skip-wad");
}

void App::run() {
    auto manager = file::IManager::make(manifest, cdn, langs);
    switch (action) {
    case Action::List:
        return list_manager(manager);
    case Action::Extract:
        return extract_manager(manager);
    case Action::Index:
        return index_manager(manager);
    }
}

void App::list_manager(std::shared_ptr<file::IManager> manager) {
    for (auto const& entry: manager->list()) {
        if (entry->is_wad()) {
            if (!skip_wad) {
                bt_trace(u8"wad: {}", entry->find_name(hashlist));
                auto wad = std::make_shared<file::ManagerWAD>(entry);
                list_manager(wad);
                continue;
            }
        }
        auto ext = entry->find_extension(hashlist);
        if (!extensions.empty() && !extensions.contains(ext)) {
            continue;
        }
        auto hash = entry->find_hash(hashlist);
        auto name = entry->find_name(hashlist);
        auto id = entry->id();
        auto size = entry->size();
        fmt_print(std::cout, u8"{:016x},{},{},{},{}\n", hash, ext, name, id, size);
    }
}

void App::extract_manager(std::shared_ptr<file::IManager> manager) {
    for (auto const& entry: manager->list()) {
        if (entry->is_wad()) {
            if (!skip_wad) {
                bt_trace(u8"wad: {}", entry->find_name(hashlist));
                auto wad = std::make_shared<file::ManagerWAD>(entry);
                extract_manager(wad);
            }
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
        auto hash = entry->find_hash(hashlist);
        auto name = entry->find_name(hashlist);
        auto out_name = name;
        if (out_name.empty() || out_name.size() > 127) {
            out_name = fmt::format(u8"{:016x}{}", hash, ext);
        }
        entry->extract_to(fs::path(output) / out_name);
    }
}

void App::index_manager(std::shared_ptr<file::IManager> manager) {
    for (auto const& entry: manager->list()) {
        if (entry->is_wad()) {
            if (!skip_wad) {
                bt_trace(u8"wad: {}", entry->find_name(hashlist));
                auto wad = std::make_shared<file::ManagerWAD>(entry);
                index_manager(wad);
            }
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
        auto hash = entry->find_hash(hashlist);
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
