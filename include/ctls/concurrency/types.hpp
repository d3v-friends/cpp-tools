#pragma once
#include <cstdint>

namespace ctls {
    enum class e_channel_status : std::uint8_t {
        init,   // 초기화 상태
        ok,     // 값 있음
        wait,   // tps 조절중으로 대기중
        empty,  // 아직 안 닫혔는데 비어 있음
        closed, // 닫혔고 남은 것도 없음
    };
}