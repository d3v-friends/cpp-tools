#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace ctls {
    // 모두 기다렸을떄 true
    // 미리 깨어났을때 false => 토큰이 만료됨. 스레드 종료신호
    inline bool
    wait_thread(const std::stop_token& own, const std::chrono::milliseconds wait) {
        std::mutex m;
        std::condition_variable_any cv;
        std::unique_lock lock{m};
        // pred가 false인 동안 대기 → 타임아웃이나 stop 요청이 오면 깨어남
        cv.wait_for(lock, own, wait, [] { return false; });
        return !own.stop_requested();
    }
} // namespace ctls