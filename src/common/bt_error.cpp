#include "bt_error.hpp"
#include <cstdio>
#include <cstdarg>

using namespace bt;
void bt::throw_error(char const* from, char const* msg) {
    throw std::runtime_error(std::string(from) + msg);
}

error_stack_t& bt::error_stack() noexcept {
    thread_local error_stack_t instance = {};
    return instance;
}

void bt::push_error_msg(std::u8string msg) noexcept {
    error_stack().emplace_back(std::move(msg));
}


void bt::push_error_msg(std::string msg) noexcept {
    error_stack().emplace_back(msg.begin(), msg.end());
}
