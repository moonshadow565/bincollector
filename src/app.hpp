#pragma once
#include <file/hashlist.hpp>
#include <file/rlsm.hpp>
#include <file/rman.hpp>
#include <file/wad.hpp>
#include <unordered_set>

struct App {
    file::HashList hashlist = {};
    std::unordered_set<std::uint64_t> rlsm_done = {};
    std::unordered_set<rman::FileID> rman_done = {};
    std::unordered_set<std::uint64_t> wad_done = {};

    void run_list(int argc, char** argv);
    void run_extract(int argc, char** argv);
    void run_extract_id(int argc, char** argv);
};
