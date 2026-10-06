#pragma once
#include <optional>
#include <type_traits>

namespace ctls {

    // enum class 의 요소, 정의된 타입으로 캐스팅 해주는 템플릿 함수
    template <typename T> constexpr auto to_enum_value(T value) noexcept {
        static_assert(std::is_enum_v<T>, "ctls::to_enum_value<T>: T must be an enum type");
        return static_cast<std::underlying_type_t<T>>(value);
    }

    template <typename T> std::optional<int> to_enum_int(T value) noexcept {
        static_assert(std::is_enum_v<T>, "ctls::to_enum_int<T>: T must be an enum type");
        static_assert(
            std::is_same_v<std::underlying_type_t<T>, int>,
            "ctls::to_enum_int<T>: underlying type of T must be int");
        return static_cast<int>(value);
    }
} // namespace ctls
