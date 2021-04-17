#pragma once
#include <stdexcept>
#include <string_view>
#include <vector>
#include <fmt/format.h>

#define bt_paste_impl(x, y) x ## y
#define bt_paste(x, y) bt_paste_impl(x, y)
#define bt_error(msg) ::bt::throw_error(__func__, ": " msg)
#define bt_assert(...) do {                                   \
        if(!(__VA_ARGS__)) {                                  \
            ::bt::throw_error(__func__, ": " #__VA_ARGS__);   \
        }                                                     \
    } while(false)
#define bt_rethrow(...)                                   \
    [&, func = __func__]() -> decltype(auto) {            \
        try {                                             \
            return __VA_ARGS__;                           \
        } catch (std::exception const &) {                \
            ::bt::throw_error(func, ": " #__VA_ARGS__); \
        }                                               \
    }()
#define bt_trace(fmtstr, ...) ::bt::ErrorTrace bt_paste(_trace_,__LINE__) {         \
        [&] { ::bt::push_error_msg(fmt::format(FMT_STRING(fmtstr), __VA_ARGS__)); } \
    }

namespace bt {
    [[noreturn]] extern void throw_error(char const* from, char const* msg);
    using error_stack_t = std::vector<std::u8string>;
    
    extern error_stack_t& error_stack() noexcept;
    
    extern void push_error_msg(std::u8string msg) noexcept;
    extern void push_error_msg(std::string msg) noexcept;
    
    template<typename Func>
    struct ErrorTrace : Func {
        inline ErrorTrace(Func&& func) noexcept : Func(std::move(func)) {}

        inline ~ErrorTrace() noexcept {
            if (std::uncaught_exceptions()) {
                Func::operator()();
            }
        }
    };
}
