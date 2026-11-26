#pragma once
#include <array>
#include <chrono>
#include <concepts>
#include <format>
#include <memory>
#include <source_location>
#include <string>
#include <utility>

#include "ctls/debug/assert.hpp"
#include "error.hpp"

namespace ctls {
    struct failed_result {
        std::string message;
        std::source_location location;
        std::chrono::system_clock::time_point timestamp;
        std::unique_ptr<failed_result> before = nullptr;

        // NOLINTNEXTLINE(readability-make-member-function-const)
        void append_before(failed_result&& value) {
            auto it = &before;
            while (*it) {
                it = &(*it)->before;
            }
            *it = std::make_unique<failed_result>(std::move(value));
        }
    };

    inline failed_result failed(
        std::string message,
        const std::source_location loc = std::source_location::current()) {
        return failed_result{
            .message = std::move(message),
            .location = loc,
            .timestamp = std::chrono::system_clock::now(),
            .before = nullptr,
        };
    }

    // 컴파일 에러
    template <typename... ARGS>
    failed_result
    failed(error::fmt_with_location<std::type_identity_t<ARGS>...> fmt, ARGS&&... args) {
        return failed_result{
            .message = std::format(fmt.fmt, std::forward<ARGS>(args)...),
            .location = fmt.location,
            .timestamp = std::chrono::system_clock::now(),
            .before = nullptr,
        };
    }

    // 런타임 에러, fmt 가 런타임에 의해 바뀌면 이것을 써야 한다.
    template <typename... ARGS>
    failed_result
    failed_runtime(const error::fmt_with_location_runtime fmt, const ARGS&... args) {
        return failed_result{
            .message = std::vformat(fmt.fmt, std::make_format_args(args...)),
            .location = fmt.location,
            .timestamp = std::chrono::system_clock::now(),
            .before = nullptr,
        };
    }

    // 입력된 순서대로 체인으로 연결된다
    failed_result fails(failed_result first, std::same_as<failed_result> auto... rest) {
        std::array<failed_result, sizeof...(rest)> ls{std::move(rest)...};

        failed_result out = std::move(first);

        for (auto& elem : ls) {
            out.append_before(std::move(elem));
        }

        return out;
    }

} // namespace ctls