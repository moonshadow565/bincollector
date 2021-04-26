#pragma once
#include <file/hashlist.hpp>
#include <file/rlsm.hpp>
#include <file/rman.hpp>
#include <file/wad.hpp>
#include <set>

struct App {
    enum class Action {
        List,
        Extract
    };

    App(fs::path hash_dir);
    fs::path hash_path_names = {};
    fs::path hash_path_extensions = {};

    file::HashList hashlist = {};
    Action action = {};
    std::u8string manifest = {};
    std::u8string cdn = {};
    std::u8string output = {};
    std::set<std::u8string> langs = {};
    std::set<std::u8string> extensions = {};

    void parse_args(int argc, char** argv);
    void run();
    void save_hashes();
private:
    void list_manager(std::shared_ptr<file::IManager> manager);
    void extract_manager(std::shared_ptr<file::IManager> manager);

};
