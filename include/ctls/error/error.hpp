#pragma once
#include <format>
#include <source_location>
#include <concepts>
#include <string_view>


// fmt 형식으로 ... args 부르면 location 자동 입력이 안되므로 한번 감싸서 처리한다.
namespace ctls::error {
    // 컴파일타임 체크 하므로 consteval 붙힌다.
    template <typename... ARGS> struct fmt_with_location {
        std::format_string<ARGS...> fmt;
        std::source_location location;

        template <class S> requires std::convertible_to<const S&, std::string_view>
        // NOLINTNEXTLINE
        consteval fmt_with_location(
            const S& s,
            const std::source_location loc =
                std::source_location::current()) : fmt{s}, location{loc} {
        }
    };


    // 런타임시 생성되므로 consteval 붙히지 않는다.
    struct fmt_with_location_runtime {
        std::string_view fmt;
        std::source_location location;

        template <class S>
            requires std::convertible_to<const S&, std::string_view>
        //NOLINTNEXTLINE
        fmt_with_location_runtime(
            const S& s,
            const std::source_location loc = std::source_location::current())
            : fmt{s}, location{loc} {
        }
    };
}