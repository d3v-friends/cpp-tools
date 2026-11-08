#pragma once
#include <string>
#include <format>
#include <utility>
#include <type_traits>
#include <optional>
#include <concepts>

#include "failed_result.hpp"
#include "status.hpp"

namespace ctls {

    // std::optional 처럼 작동하지만 에러나 날땐 메세지도 같이 전달
    template <class T> class [[nodiscard]] result : public status {
    public:
        // explicit 안써야 자동 형변환 가능
        // NOLINTNEXTLINE
        result(failed_result f) : status{std::move(f)} {
        }

        // T 로 암시적으로 변환되는 것을 모두 받아처리
        // explicit 을 안써줘야 자동 형변환 가능
        template <class U = T>
            requires(std::convertible_to<U&&, T> &&
                     !std::same_as<std::remove_cvref_t<U>, failed_result>)
        // NOLINTNEXTLINE
        result(U&& value) : status(), _value{std::in_place, std::forward<U>(value)} {
        }

        // operator, 값이없는데 값을 가져오려하면 UB
        T& operator*() & { return *_value; }
        const T& operator*() const & { return *_value; }
        T&& operator*() && { return std::move(*_value); }
        T* operator->() { return &*_value; }
        const T* operator->() const { return &*_value; }

    protected:
        std::optional<T> _value;
    };
} // namespace ctls