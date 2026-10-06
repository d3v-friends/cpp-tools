#pragma once
#include <chrono>

namespace ctls {
    inline float diff_milliseconds(
        const std::chrono::steady_clock::time_point a,
        const std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<float, std::milli>(a - b).count();
    }

    inline float diff_seconds(
        const std::chrono::steady_clock::time_point a,
        const std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<float>(a - b).count();
    }
} // namespace ctls
