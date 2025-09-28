#pragma once

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>

#define TRY_ASSIGN(var, expr)                            \
    do {                                                 \
        auto&& _try_result = (expr);                     \
        if (!_try_result) {                              \
            return std::unexpected(_try_result.error()); \
        }                                                \
        var = std::move(*_try_result);                   \
    } while (0)

#define TRY(expr)                                        \
    do {                                                 \
        auto&& _try_result = (expr);                     \
        if (!_try_result) {                              \
            return std::unexpected(_try_result.error()); \
        }                                                \
    } while (0)

namespace thesis::host::utils {

struct Unit {
    constexpr bool operator==(Unit) const noexcept { return true; }
    constexpr bool operator!=(Unit) const noexcept { return false; }
};

namespace detail {
inline std::string last_error_string() {
#ifdef _MSC_VER
    std::array<char, 256> buf{};
    strerror_s(buf.data(), buf.size(), errno);
    return std::string(buf.data());
#else
    return std::string(strerror(errno));
#endif
}
}  // namespace detail

struct Error {
    int code_;
    std::string msg_;

    Error(int code, std::string_view msg) : code_(code), msg_(msg) {}
    Error(int code, std::string&& msg) : code_(code), msg_(std::move(msg)) {}
    explicit Error(std::string_view msg) : Error(errno, msg) {}

    static Error fromErrno() { return Error(errno, std::string_view(detail::last_error_string())); }

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

// TODO(kacper): think of using in places that use the CHECK macros
template <typename T = Unit>
using Result = std::expected<T, Error>;

template <typename T = Unit>
T try_unwrap_or_exit(Result<T>&& res) {
    if (!res) {
        const Error& err = res.error();
        spdlog::error("Fatal: {} (code {})", err.msg_, err.code_);
        std::exit(err.code_);
    }
    return std::move(*res);
}

template <typename... Args>
std::unexpected<Error> make_error(Args&&... args) {
    return std::unexpected<Error>(Error(std::forward<Args>(args)...));
}

}  // namespace thesis::host::utils

template <>
struct fmt::formatter<thesis::host::utils::Error> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const thesis::host::utils::Error& err, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "[code={}] {}", err.code_, err.msg_);
    }
};
