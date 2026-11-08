#pragma once
#include <string>
#include <format>
#include <utility>
#include <source_location>
#include <chrono>

#include "error.hpp"

namespace ctls {
    struct failed_result {
        std::string message;
        std::source_location location;
        std::chrono::system_clock::time_point timestamp;
    };

    inline failed_result failed(
        std::string message,
        std::source_location loc = std::source_location::current()) {
        return failed_result{
            .message = std::move(message),
            .location = loc,
            .timestamp = std::chrono::system_clock::now(),
        };
    }

    // 컴파일 에러
    template <typename... ARGS>
    failed_result failed(
        error::fmt_with_location<std::type_identity_t<ARGS>...> fmt,
        ARGS&&... args) {
        return failed_result{
            .message = std::format(fmt.fmt, std::forward<ARGS>(args)...),
            .location = fmt.location,
            .timestamp = std::chrono::system_clock::now(),
        };
    }

    // 런타임 에러, fmt 가 런타임에 의해 바뀌면 이것을 써야 한다.
    template <typename... ARGS>
    failed_result failed_runtime(
        error::fmt_with_location_runtime fmt,
        const ARGS&... args) {
        return failed_result{
            .message = std::vformat(fmt.fmt, std::make_format_args(args...)),
            .location = fmt.location,
            .timestamp = std::chrono::system_clock::now(),
        };
    }

} // namespace ctls