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

    App(fs::path src_dir);
    fs::path src_dir;
    file::HashList hashlist = {};
    Action action = {};
    std::u8string manifest = {};
    std::u8string cdn = {};
    std::u8string output = {};
    std::set<std::u8string> langs = {};
    std::set<std::u8string> extensions = {};
    std::u8string hash_path_names = {};
    std::u8string hash_path_extensions = {};

    void parse_args(int argc, char** argv);
    void load_hashes();
    void run();
    void save_hashes();
private:
    void list_manager(std::shared_ptr<file::IManager> manager);
    void extract_manager(std::shared_ptr<file::IManager> manager);

};
