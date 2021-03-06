#pragma once
#include <file/hashlist.hpp>
#include <file/rlsm.hpp>
#include <file/rman.hpp>
#include <file/wad.hpp>
#include <set>

struct App {
    struct Action {
        void (App::* handler)(std::shared_ptr<file::IManager>, int depth);
        std::string_view long_name;
        std::optional<std::string_view> short_name = std::nullopt;
        bool has_hashes = true;
    };

    App(fs::path src_dir);
    fs::path src_dir;
    file::HashList hashlist = {};
    Action action = {};
    std::u8string manifest = {};
    std::u8string cdn = {};
    std::u8string output = {};
    std::u8string remote = {};
    std::set<std::u8string> langs = {};
    std::set<std::u8string> extensions = {};
    std::set<std::uint64_t> names = {};
    std::u8string hash_path_names = {};
    std::u8string hash_path_extensions = {};
    int max_depth = {};
    bool show_wads = {};
    bool skip_root = {};

    void parse_args(int argc, char** argv);
    void load_hashes();
    void run();
    void save_hashes();
private:
    void checksum_manager(std::shared_ptr<file::IManager> manager, int depth);
    void list_manager(std::shared_ptr<file::IManager> manager, int depth);
    void extract_manager(std::shared_ptr<file::IManager> manager, int depth);
    void index_manager(std::shared_ptr<file::IManager> manager, int depth);
    void exe_ver(std::shared_ptr<file::IManager> manager, int depth);

    static inline constexpr Action ACTIONS[] = {
        { &App::list_manager, "list", "ls", true },
        { &App::extract_manager, "extract", "ex", true },
        { &App::index_manager, "index", std::nullopt, true },
        { &App::exe_ver, "exever", std::nullopt, false },
        { &App::checksum_manager, "checksum", std::nullopt, true },
    };
};
