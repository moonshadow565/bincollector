#include <common/bt_error.hpp>
#include <common/string.hpp>
#include <iostream>
#include "app.hpp"

int main(int argc, char** argv) {
    auto app = App(fs::path(argv[0]).parent_path());
    try {
        app.parse_args(argc, argv);
        app.load_hashes();
        app.run();
        app.save_hashes();
        return EXIT_SUCCESS;
    } catch (std::runtime_error const& error) {
        std::cerr << "Error: " <<  error.what() << std::endl;
        for (auto const& entry: bt::error_stack()) {
            std::cerr << "\t" << to_std_string(entry) << std::endl;
        }
        bt::error_stack().clear();
        return EXIT_FAILURE;
    }
}
