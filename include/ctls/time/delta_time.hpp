#pragma once
#include <chrono>
#include <mutex>

namespace ctls {
    class delta_time {
    public:
        explicit delta_time() : _tick(std::chrono::steady_clock::now()) {}
        ~delta_time() = default;
        delta_time(const delta_time&) = delete;
        delta_time& operator=(const delta_time&) = delete;
        delta_time(delta_time&&) = delete;
        delta_time& operator=(delta_time&&) = delete;

        void set_fps(const float fps) noexcept {
            std::scoped_lock lock(_mutex);
            _fps = fps;
            _frame_budget = 1.0f / _fps;
        }

        [[nodiscard]] float get_fps() noexcept {
            std::scoped_lock lock(_mutex);
            return _fps;
        }

        [[nodiscard]] float get_delta_time() {
            const auto tick = std::chrono::steady_clock::now();
            std::chrono::duration<float> delay = tick - _tick;

            // 남은 시간 = 예산 - 이미 지난 시간
            const std::chrono::duration<float> remaining(_frame_budget - delay.count());

            if (constexpr auto margin = std::chrono::milliseconds(2);
                margin < remaining) {
                std::this_thread::sleep_for(remaining - margin);
            }

            // mTick 기준으로 계속 누적 측정 (기준점 리셋 금지)
            delay = std::chrono::steady_clock::now() - _tick;
            while (delay.count() < _frame_budget) {
                std::this_thread::yield();
                delay = std::chrono::steady_clock::now() - _tick;
            }

            _tick = std::chrono::steady_clock::now();
            return std::min(delay.count(), _max_delta_time);
        }

    private:
        mutable std::mutex _mutex;
        std::chrono::time_point<std::chrono::steady_clock> _tick;
        float _fps = 60.0f;
        float _frame_budget = 1.0f / _fps;
        float _max_delta_time = 0.1f;
    };

} // namespace ctls