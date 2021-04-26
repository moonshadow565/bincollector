#include <common/bt_error.hpp>
#include <common/string.hpp>
#include "app.hpp"

int main(int argc, char** argv) {
    try {
        auto app = App(fs::path(argv[0]).parent_path());
        app.parse_args(argc, argv);
        app.run();
        app.save_hashes();
        return EXIT_SUCCESS;
    } catch (std::runtime_error const& error) {
        fmt::print("Error: {}\n", error.what());
        for (auto const& entry: bt::error_stack()) {
            fmt::print("\t{}\n", to_std_string(entry));
        }
        bt::error_stack().clear();
        return EXIT_FAILURE;
    }
}
