#pragma once
#include <array>
#include <concepts>

#include "status.hpp"

namespace ctls {
    // 실패한 것만 역순으로 체인으로 묶는다. (e1, e2, e3) → e3 → e2 → e1
    // 전부 성공이거나 인자가 없으면 ok()
    template <std::convertible_to<status>... Ts> status make_statuses(Ts&&... ls) {
        std::array<status, sizeof...(Ts)> arr{status(std::forward<Ts>(ls))...};

        status out = ok();
        for (auto& s : arr) {
            if (s.is_ok()) {
                continue;
            }
            if (!out.is_ok()) {
                s.append_chain(std::move(out)); // 이전 실패들을 뒤에 매단다
            }
            out = std::move(s); // 최신 실패가 맨 앞
        }
        return out;
    }
} // namespace ctls