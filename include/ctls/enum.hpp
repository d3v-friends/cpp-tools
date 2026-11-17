#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace ctls {
    // enum class 마지막에 _count 를 추가하면 갯수를 자동으로 세어준다.
    template <typename E>
        requires std::is_enum_v<E>
    inline constexpr std::size_t enum_count_v = static_cast<std::size_t>(E::_count);

    template <typename E, typename Info, std::size_t N>
        requires std::is_enum_v<E>
    constexpr std::optional<E> enum_find(
        const std::array<Info, N>& list,
        std::string_view Info::* field,
        std::string_view s) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            if (list[i].*field == s) {
                return static_cast<E>(i);
            }
        }
        return std::nullopt;
    }

    template <typename E>
        requires std::is_enum_v<E>
    constexpr std::optional<E> from_key(std::string_view s) noexcept {
        return from_key(s, std::type_identity<E>{});
    }
} // namespace ctls