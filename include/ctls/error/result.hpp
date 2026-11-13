#pragma once
#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "failed_result.hpp"

namespace ctls {

    // std::optional 처럼 작동하지만 에러나 날땐 메세지도 같이 전달
    template <class T> class [[nodiscard]] result {
    public:
        // explicit 안써야 자동 형변환 가능
        // NOLINTNEXTLINE
        result(failed_result f)
            : _failed{std::make_unique<failed_result>(std::move(f))} {}

        // T 로 암시적으로 변환되는 것을 모두 받아처리
        // explicit 을 안써줘야 자동 형변환 가능
        template <class U = T>
            requires(
                std::convertible_to<U&&, T> &&
                !std::same_as<std::remove_cvref_t<U>, failed_result>)
        // NOLINTNEXTLINE
        result(U&& value) : _value{std::in_place, std::forward<U>(value)} {}

        result(const result&) = delete;
        result& operator=(const result&) = delete;
        result(result&&) = default;
        result& operator=(result&&) = default;

        // bool 판별식
        [[nodiscard]] explicit operator bool() const noexcept {
            return _value.has_value();
        }

        [[nodiscard]] bool has_value() const noexcept {
            return _value.has_value();
        }

        // NOLINTNEXTLINE
        failed_result get_failed() && {
            IASSERT(_failed != nullptr, "not failed result");
            return std::move(*_failed);
        }

        // operator, 값이없는데 값을 가져오려하면 UB
        T& operator*() & {
            return *_value;
        }

        const T& operator*() const& {
            return *_value;
        }

        T operator*() && {
            return std::move(*_value);
        }

        T* operator->() {
            return &*_value;
        }

        const T* operator->() const {
            return &*_value;
        }

    protected:
        std::optional<T> _value;
        std::unique_ptr<failed_result> _failed = nullptr;
    };

    // result<void> 특수화
    struct ok_result {
        explicit constexpr ok_result() = default; // {} 가 ok_result 로 잘못 변환되는 걸 막음
    };

    inline constexpr ok_result ok{};

    // 값이 없는 결과. 성공은 return {}; 실패는 return failed(...);
    template <> class [[nodiscard]] result<void> {
    public:
        // 성공
        result() = default;

        result(ok_result) noexcept {}

        // NOLINTNEXTLINE
        result(failed_result f)
            : _failed{std::make_unique<failed_result>(std::move(f))} {}

        result(const result&) = delete;
        result& operator=(const result&) = delete;
        result(result&&) = default;
        result& operator=(result&&) = default;

        [[nodiscard]] explicit operator bool() const noexcept {
            return _failed == nullptr;
        }

        // NOLINTNEXTLINE
        failed_result get_failed() && {
            IASSERT(_failed != nullptr, "not failed result");
            return std::move(*_failed);
        }

    protected:
        std::unique_ptr<failed_result> _failed = nullptr;
    };

} // namespace ctls