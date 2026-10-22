#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <format>

#include "diff.hpp"
#include "ctls/debug/logger.hpp"

namespace ctls {
    class stop_watch {
    public:
        explicit stop_watch()
            : _start_at(std::chrono::steady_clock::now()),
              _last_print_at(std::chrono::steady_clock::now()), _time_points{_start_at} {}

        ~stop_watch() = default;
        stop_watch(const stop_watch&) = delete;
        stop_watch& operator=(const stop_watch&) = delete;
        stop_watch(stop_watch&&) = delete;
        stop_watch& operator=(stop_watch&&) = delete;

        void reset() {
            std::scoped_lock lock(_mutex);
            _start_at = std::chrono::steady_clock::now();

            const auto last_time_point_size = _time_points.size();
            _time_points.clear();
            _time_points.reserve(last_time_point_size);
            _time_points.push_back(_start_at);

            const auto last_keys_size = _keys.size();
            _keys.clear();
            _keys.reserve(last_keys_size);
        }

        void check(const std::string& key) {
            std::scoped_lock lock(_mutex);
            _time_points.push_back(std::chrono::steady_clock::now());
            _keys.push_back(key);
        }

        void print() {
            std::scoped_lock lock(_mutex);
            if (diff_milliseconds(_start_at, _last_print_at) < _duration) [[likely]] {
                return;
            }

            std::unordered_map<std::string, float> map;
            get_all_unlocked(map);
            std::string log;
            for (const auto& [key, time] : map) {
                log += std::format(", {}={:5f}", key, time);
            }

            ILOG_TRACE("fps={:1f}, budget={:5f}{}", _fps, _frame_budget, log);

            _last_print_at = _start_at;
        }

        void get_all(std::unordered_map<std::string, float>& out) const {
            std::scoped_lock lock(_mutex);
            get_all_unlocked(out);
        }

        void set_fps(const float fps) {
            std::scoped_lock lock(_mutex);
            _fps = fps;
            _frame_budget = 1.0f / _fps;
        }

        void set_print_duration_milliseconds(const float duration) {
            std::scoped_lock lock(_mutex);
            _duration = duration;
        }

    private:
        mutable std::mutex _mutex;
        std::vector<std::string> _keys;
        std::chrono::steady_clock::time_point _start_at;
        std::chrono::steady_clock::time_point _last_print_at;
        std::vector<std::chrono::steady_clock::time_point> _time_points;
        float _duration = 3000.0f;
        float _fps = 60.0f;
        float _frame_budget = 1.0f / _fps;

        void get_all_unlocked(std::unordered_map<std::string, float>& out) const {
            int index = 0;
            for (const auto& key : _keys) {
                out.emplace(
                    key, diff_milliseconds(
                             _time_points.at(index + 1), _time_points.at(index)));
                index += 1;
            }
        }
    };
} // namespace ctls
