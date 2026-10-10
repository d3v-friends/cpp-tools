#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

#include "diff.hpp"
#include "ctls/debug/logger.hpp"

namespace ctls {
    class stop_watch {
    public:
        explicit stop_watch()
            : m_start_at(std::chrono::steady_clock::now()),
              m_last_print_at(std::chrono::steady_clock::now()) {}

        ~stop_watch() = default;
        stop_watch(const stop_watch&) = delete;
        stop_watch& operator=(const stop_watch&) = delete;
        stop_watch(stop_watch&&) = delete;
        stop_watch& operator=(stop_watch&&) = delete;

        void reset() {
            std::scoped_lock lock(m_mutex);
            m_start_at = std::chrono::steady_clock::now();

            const auto last_time_point_size = m_time_points.size();
            m_time_points.clear();
            m_time_points.reserve(last_time_point_size);
            m_time_points.push_back(m_start_at);

            const auto last_keys_size = m_keys.size();
            m_keys.clear();
            m_keys.reserve(last_keys_size);
        }

        void check(const std::string& key) {
            std::scoped_lock lock(m_mutex);
            m_time_points.push_back(std::chrono::steady_clock::now());
            m_keys.push_back(key);
        }

        void print() {
            std::scoped_lock lock(m_mutex);
            if (diff_milliseconds(m_start_at, m_last_print_at) < m_duration) [[likely]] {
                return;
            }

            std::unordered_map<std::string, float> map;
            get_all_unlocked(map);
            std::string log;
            for (const auto& [key, time] : map) {
                log += std::format(", {}={:5f}", key, time);
            }

            I_LOG_F("fps={:1f}, budget={:5f}{}", m_fps, m_frame_budget, log);

            m_last_print_at = m_start_at;
        }

        void get_all(std::unordered_map<std::string, float>& out) {
            std::scoped_lock lock(m_mutex);
            get_all_unlocked(out);
        }

        void set_fps(const float fps) {
            std::scoped_lock lock(m_mutex);
            m_fps = fps;
            m_frame_budget = 1.0f / m_fps;
        }

        void set_print_duration_milliseconds(const float duration) {
            std::scoped_lock lock(m_mutex);
            m_duration = duration;
        }

    private:
        std::mutex m_mutex;
        std::vector<std::chrono::steady_clock::time_point> m_time_points;
        std::vector<std::string> m_keys;
        std::chrono::steady_clock::time_point m_start_at;
        std::chrono::steady_clock::time_point m_last_print_at;
        float m_duration = 3000.0f;
        float m_fps = 60.0f;
        float m_frame_budget = 1.0f / m_fps;

        void get_all_unlocked(std::unordered_map<std::string, float>& out) const {
            int index = 0;
            for (const auto& key : m_keys) {
                out.emplace(
                    key, diff_milliseconds(
                             m_time_points.at(index + 1), m_time_points.at(index)));
                index += 1;
            }
        }
    };
} // namespace ctls
