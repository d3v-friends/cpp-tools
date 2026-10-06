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
        explicit stop_watch() = default;
        ~stop_watch();
        stop_watch(const stop_watch&) = delete;
        stop_watch& operator=(const stop_watch&) = delete;
        stop_watch(stop_watch&&) = delete;
        stop_watch& operator=(stop_watch&&) = delete;

        void reset() {
            std::scoped_lock lock(m_mutex);

            m_start_at = std::chrono::steady_clock::now();
            m_time_points.clear();
            m_time_points.reserve(m_last_count);
            m_time_points.push_back(m_start_at);
            m_keys.clear();
            m_time_points.reserve(m_last_count - 1);
        }

        void check(const std::string& key) {
            std::scoped_lock lock(m_mutex);
            m_time_points.push_back(std::chrono::steady_clock::now());
            m_keys.push_back(key);
        }

        void print() {
            if (diff_milliseconds(m_start_at, m_last_print_at) < m_duration) [[likely]] {
                return;
            }

            std::unordered_map<std::string, float> map;
            get_all(map);
            std::string log;
            for (const auto& [key, time] : map) {
                log += std::format("{}={:5f}", key, time);
            }
            I_LOG_F("fps={:1f}, budget={:5f}", m_fps, m_frame_budget, log.c_str());

            m_last_print_at = m_start_at;
            m_last_count = m_time_points.size();
            m_time_points.clear();
            m_keys.clear();
        }

        void get_all(std::unordered_map<std::string, float>& out) const {
            int index = 0;
            for (const auto& key : m_keys) {
                out.emplace(
                    key, diff_milliseconds(
                             m_time_points.at(index + 1), m_time_points.at(index)));
                index += 1;
            }
        }

    private:
        std::mutex m_mutex;
        std::vector<std::chrono::steady_clock::time_point> m_time_points;
        std::vector<std::string> m_keys;
        std::chrono::steady_clock::time_point m_start_at;
        std::chrono::steady_clock::time_point m_last_print_at;
        size_t m_last_count = 1;
        float m_duration = 0;
        float m_fps = 0;
        float m_frame_budget = 0;
    };
} // namespace ctls
