#pragma once
#include <optional>
#include <typeinfo>
#include <type_traits>

namespace ctls {
    // 클래스 비교만 가능하다. 상위 클래스까지 검색한다. 하위 클래스라도 인정
    template <typename T, typename U> std::optional<T*> dynamic(U* u) noexcept {
        static_assert(std::is_base_of_v<U, T>, "ctls::dynamic<T>: T must derive from U");

        if (!u) [[unlikely]] {
            return std::nullopt;
        }

        if (T* casted = dynamic_cast<T*>(u)) [[likely]] {
            return casted;
        }
        return std::nullopt;
    }

    // typeid 로 비교한다. 한번에 비교, 정확히 동일 타입일때만 인정
    // typeid 는 내부적으로 실패시 프로그램전체가 바로 terminate 되어버린다.
    template <typename T, typename U> std::optional<T*> type_id(U* u) noexcept {
        static_assert(std::is_base_of_v<U, T>, "ctls::type_id<T>: T must derive from U");

        if (!u) [[unlikely]] {
            return std::nullopt;
        }

        if (typeid(T) != typeid(*u)) [[unlikely]] {
            return std::nullopt;
        }

        return static_cast<T*>(u);
    }
} // namespace ctls
