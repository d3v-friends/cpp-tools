#pragma once
#include <cstddef>
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

/*
 * // 숫자를 직접 적어두면 실수로 순서를 바꿔도 diff에서 바로 보여
 * enum class e_user_status : std::uint8_t {
 *     wait_tdlib_parameters = 0, wait_phone_number = 1, wait_code = 2, wait_password=***
 *     ready = 4, logging_out = 5, closing = 6, closed = 7,
 *     _count
 * };
 * struct e_user_status_info {
 *     std::string_view key; // 고정: 저장·통신용, 절대 변경 금지
 *     std::string_view kr;  // 표시용: 자유롭게 변경
 *     std::string_view en;  // 표시용: 자유롭게 변경
 * };
 * inline constexpr std::array<e_user_status_info, ctls::enum_count_v<e_user_status>>
 * e_user_status_info_list{{
 *     {.key = "wait_tdlib_parameters", .kr = "초기화",      .en = "Initializing"},
 *     {.key = "wait_phone_number",     .kr = "전화번호",    .en = "Phone number"},
 *     {.key = "wait_code",             .kr = "인증코드",    .en = "Code"},
 *     {.key = "wait_password",         .kr = "2차인증",     .en = "Password"},
 *     {.key = "ready",                 .kr = "사용중",      .en = "Ready"},
 *     {.key = "logging_out",           .kr = "로그아웃 중", .en = "Logging out"},
 *     {.key = "closing",               .kr = "사용종료 중", .en = "Closing"},
 *     {.key = "closed",                .kr = "사용종료",    .en = "Closed"},
 * }};
 * static_assert(std::ranges::none_of(e_user_status_info_list,
 *                                    [](const e_user_status_info& i) { return
 * i.key.empty(); }),
 *               "e_user_status_info_list: missing row");
 * constexpr std::string_view to_key(e_user_status v) noexcept
 * { return e_user_status_info_list[static_cast<std::size_t>(v)].key; }
 * constexpr std::string_view to_kr(e_user_status v) noexcept
 * { return e_user_status_info_list[static_cast<std::size_t>(v)].kr; }
 * constexpr std::string_view to_en(e_user_status v) noexcept
 * { return e_user_status_info_list[static_cast<std::size_t>(v)].en; }
 *
 *
 */