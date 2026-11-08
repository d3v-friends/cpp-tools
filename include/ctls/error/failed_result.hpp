#pragma once
#include <string>
#include <format>
#include <string_view>
#include <utility>

namespace ctls {
    struct failed_result {
        std::string message;
    };

    inline failed_result failed(std::string message) {
        return failed_result{
            .message = std::move(message),
        };
    }

    // 컴파일 에러
    template <typename... ARGS>
    failed_result failed(std::format_string<ARGS...> fmt, ARGS&&... args) {
        return failed_result{
            .message = std::format(fmt, std::forward<ARGS>(args)...),
        };
    }

    // 런타임 에러, fmt 가 런타임에 의해 바뀌면 이것을 써야 한다.
    template <typename... ARGS>
    failed_result failed_runtime(const std::string_view fmt, const ARGS&... args) {
        return failed_result{
            .message = std::vformat(fmt, std::make_format_args(args...)),
        };
    }

} // namespace ctls