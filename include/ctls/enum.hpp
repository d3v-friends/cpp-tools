#pragma once
#include <cstddef>
#include <type_traits>

namespace ctls {

    // enum class 마지막에 _count 를 추가하면 갯수를 자동으로 세어준다.
    template <typename E>
        requires std::is_enum_v<E>
    inline constexpr std::size_t enum_count_v = static_cast<std::size_t>(E::_count);

} // namespace ctls