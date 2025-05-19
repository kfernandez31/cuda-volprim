#pragma once

#include <cerrno>
#include <cstdlib>
#include <expected>
#include <spdlog/fmt/fmt.h>
#include <string>
#include <string_view>
#include <utility>

#define TRY(expr)                                             \
    ({                                                        \
        auto&& __res = (expr);                                \
        if (!__res)                                           \
            return std::unexpected(std::move(__res.error())); \
        std::move(*__res);                                    \
    })

// TODO(kacper): worth adding a nested namespace?
namespace thesis {

struct Unit {
    constexpr bool operator==(Unit) const noexcept { return true; }
    constexpr bool operator!=(Unit) const noexcept { return false; }
};

struct Error;

template <typename T>
using Result = std::expected<T, Error>;

template <typename T = void>
T try_unwrap_or_exit(std::expected<T, Error>&& res) {
    if (!res) {
        const Error& err = res.error();
        spdlog::error("Fatal: {} (code {})", err.msg_, err.code_);
        std::exit(err.code_);
    }
    return std::move(*res);
}

struct Error {
    int code_;
    std::string msg_;

    Error(int code, std::string_view msg) : code_(code), msg_(msg) {}
    explicit Error(std::string_view msg) : Error(errno, msg) {}

    Error(int code, const std::string& msg) : Error(code, std::string_view(msg)) {}
    Error(int code, std::string&& msg) : code_(code), msg_(std::move(msg)) {}

    template <typename... Args>
    Error(int code, fmt::format_string<Args...> fmt_str, Args&&... args)
        : code_(code), msg_(fmt::format(fmt_str, std::forward<Args>(args)...)) {}

    template <typename... Args>
    Error(int code, std::string_view fmt_str, Args&&... args)
        : code_(code), msg_(fmt::format(fmt::runtime(fmt_str), std::forward<Args>(args)...)) {}

    template <typename... Args>
    Error(fmt::format_string<Args...> fmt_str, Args&&... args)
        : Error(errno, fmt_str, std::forward<Args>(args)...) {}

    template <typename... Args>
    Error(std::string_view fmt_str, Args&&... args)
        : Error(errno, fmt_str, std::forward<Args>(args)...) {}
};

}  // namespace thesis
