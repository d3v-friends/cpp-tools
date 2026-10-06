#pragma once
#include <chrono>
#include <mutex>

namespace ctls {
    class delta_time {
    public:
        delta_time();
        ~delta_time();
        delta_time(const delta_time&) = delete;
        delta_time& operator=(const delta_time&) = delete;
        delta_time(delta_time&&) = delete;
        delta_time& operator=(delta_time&&) = delete;

        void set_fps(const float fps) noexcept {
            std::scoped_lock lock(m_mutex);
            m_fps = fps;
            m_frame_budget = 1.0f / m_fps;
        }

        [[nodiscard]] float get_fps() noexcept {
            std::scoped_lock lock(m_mutex);
            return m_fps;
        }

        [[nodiscard]] float get_delta_time() {
            const auto tick = std::chrono::steady_clock::now();
            std::chrono::duration<float> delay = tick - m_tick;

            // 남은 시간 = 예산 - 이미 지난 시간
            const std::chrono::duration<float> remaining(m_frame_budget - delay.count());

            if (constexpr auto margin = std::chrono::milliseconds(2);
                margin < remaining) {
                std::this_thread::sleep_for(remaining - margin);
            }

            // mTick 기준으로 계속 누적 측정 (기준점 리셋 금지)
            delay = std::chrono::steady_clock::now() - m_tick;
            while (delay.count() < m_frame_budget) {
                std::this_thread::yield();
                delay = std::chrono::steady_clock::now() - m_tick;
            }

            m_tick = std::chrono::steady_clock::now();
            return std::min(delay.count(), m_max_delta_time);
        }

    private:
        std::mutex m_mutex;
        std::chrono::time_point<std::chrono::steady_clock> m_tick;
        float m_fps = 60.0f;
        float m_frame_budget = 1.0f / m_fps;
        float m_max_delta_time = 0.1f;
    };

} // namespace ctls