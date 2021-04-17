#include <common/bt_error.hpp>
#include <file/base.hpp>
#include <file/wad.hpp>
#include <file/hashlist.hpp>

static std::string str(std::u8string s) { return {s.begin(), s.end()}; }

int main(int argc, char** argv) {
    try {
        auto hashlist = file::HashList{};
        hashlist.read_names_list("hashes.game.txt");
        hashlist.read_extensions_list("hashes.ext.game.txt");
        auto const path = fs::path(argc > 1 ? argv[1] : "");
        auto const cdn = fs::path(argc > 2 ? argv[2] : "");
        auto file_list = file::IFile::list_path(path, cdn);
        for (auto& entry: file_list) {
            auto const hash = entry->find_hash(hashlist);
            auto const extension = entry->find_extension(hashlist);
            auto const name = entry->find_name(hashlist);
            auto const link = entry->get_link();
            fmt::print("{:016X},{},{},{}\n", hash, str(extension), str(name), str(link));
            if (link.empty() && (extension == u8".client" || extension == u8".wad")) {
                auto wad_list = file::FileWAD::list_wad(entry->open());
                for (auto& entry: wad_list) {
                    auto const hash = entry->find_hash(hashlist);
                    auto const extension = entry->find_extension(hashlist);
                    auto const name = entry->find_name(hashlist);
                    auto const link = entry->get_link();
                    fmt::print("\t{:016X},{},{},{}\n", hash, str(extension), str(name), str(link));
                }
            }
        }
    } catch (std::runtime_error const& error) {
        fmt::print("Error: {}\n", error.what());
        for (auto const& entry: bt::error_stack()) {
            fmt::print("\t{}\n", str(entry));
        }
        bt::error_stack().clear();
    }
    return 0;
}
