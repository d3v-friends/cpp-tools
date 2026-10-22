#pragma once
#include <cstdint>
#include <mutex>
#include <deque>
#include <format>
#include <vector>
#include <source_location>
#include <thread>
#include <string>
#include <string_view>
#include <chrono>
#include <ctime>
#include <functional>
#include <iterator>
#include <cstddef>

#include "types.hpp"

namespace ctls {
    // 전역에서 데이터를 받아서 데이터를 저장한다.
    // main 문에서 로그레벨 설정과 매크로 한번 사용해주는 것이 좋다.
    // 생성 순서에 따라서 소멸자가 실행되는것의 순서를 맞추기위 해 꼭 필요함.
    class p_logger {
    public:
        p_logger(const p_logger&) = delete;
        p_logger& operator=(const p_logger&) = delete;
        p_logger(p_logger&&) = delete;
        p_logger& operator=(p_logger&&) = delete;

        static p_logger& instance() {
            static p_logger i;
            return i;
        }

        // 1 이상의 값만 입력할수 있다.
        static void set_max_log(const std::size_t max_log) {
            if (max_log == 0) [[unlikely]] {
                return;
            }
            auto& self = instance();
            std::scoped_lock lock{self._mutex};
            self._max_log = max_log;
            while (self._max_log < self._logs.size()) { // 둘 다 size_t라 캐스팅 불필요
                self._logs.pop_front();
            }
        }

        static void set_log_level(const e_log_level level) {
            auto& self = instance();
            std::scoped_lock lock{self._mutex};
            self._level = level;
        }

        static std::vector<std::string> get_logs() {
            auto& self = instance();
            std::scoped_lock lock{self._mutex};
            std::vector<std::string> logs;
            logs.reserve(self._logs.size());
            for (const auto& log : self._logs) {
                logs.push_back(log);
            }
            return logs;
        }

        template <typename... ARGS>
        static void print(
            const e_log_level level,
            const std::source_location& location,
            std::format_string<ARGS...> fmt,
            ARGS&&... args) {
            auto& self = instance();
            // 로그레벨 확인
            std::unique_lock lock{self._mutex};
            if (level < self._level) {
                return;
            }
            lock.unlock();

            // 타임스탬프 찍기
            const auto tp = std::chrono::system_clock::now();
            const auto now = std::chrono::system_clock::to_time_t(tp);
            const auto ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    tp.time_since_epoch())
                    .count() %
                1000);

            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &now); // MSVC: (tm*, time_t*)
#else
            localtime_r(&now, &local); // POSIX: (time_t*, tm*)
#endif

            char time_text[34];
            const auto time_len = std::strftime(
                time_text, sizeof(time_text), "[%Y-%m-%d][%H:%M:%S]", &local);

            // 시스템 위치
            std::string_view filename{location.file_name()};
            if (const auto pos = filename.find_last_of("/\\");
                pos != std::string_view::npos) {
                filename.remove_prefix(pos + 1); // view의 시작 위치만 옮김 (복사 없음)
            }

            const auto thread_tag =
                std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF;

            std::string message;
            message.reserve(192);
            std::format_to(
                std::back_inserter(message), "{}[{:03}]{}[{:04x}] ",
                std::string_view{time_text, time_len}, // [2026-10-22][14:03:27]
                ms,                                    // [042]  ← 3자리 0채움
                to_string(level), thread_tag);
            std::format_to(std::back_inserter(message), fmt, std::forward<ARGS>(args)...);
            std::format_to(
                std::back_inserter(message), " ({}({}))", filename, location.line());

            // 데이터 입력
            lock.lock();
            self._logs.push_back(std::move(message));
            while (self._max_log < self._logs.size()) { // 둘 다 size_t라 캐스팅 불필요
                self._logs.pop_front();
            }
        }

    private:
        explicit p_logger() = default;
        ~p_logger() = default;
        std::mutex _mutex;
        std::size_t _max_log = 100;
        std::deque<std::string> _logs;
        e_log_level _level = e_log_level::trace;
    };

} // namespace ctls

#define PLOG(level, ...)                                                                 \
    ::ctls::p_logger::print(level, std::source_location::current(), __VA_ARGS__)

#ifdef NDEBUG
#define PLOG_TRACE(...) ((void)0)
#define PLOG_DEBUG(...) ((void)0)
#define PLOG_INFO(...) ((void)0)
#else
#define PLOG_TRACE(...) PLOG(::ctls::e_log_level::trace, __VA_ARGS__)
#define PLOG_DEBUG(...) PLOG(::ctls::e_log_level::debug, __VA_ARGS__)
#define PLOG_INFO(...) PLOG(::ctls::e_log_level::info, __VA_ARGS__)
#endif

#define PLOG_WARN(...) PLOG(::ctls::e_log_level::warning, __VA_ARGS__)
#define PLOG_ERROR(...) PLOG(::ctls::e_log_level::error, __VA_ARGS__)