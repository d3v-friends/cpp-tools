#pragma once
#include <format>
#include <source_location>
#include <string_view>
#include <thread>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>

#include "ctls/logger/logger.hpp"
#include "types.hpp"

namespace ctls {
    inline void print_line(const std::source_location& loc, std::string_view str) {
        const auto line = std::format("[{}:{}] {}\n", loc.file_name(), loc.line(), str);
        print(line);
    }

    template <typename... Args>
    void printf_line(
        const std::source_location& location,
        std::format_string<Args...> fmt,
        Args&&... args) {
        const auto line = std::format(
            "[{:x}] {}: {}", std::hash<std::thread::id>{}(std::this_thread::get_id()),
            location.function_name(),
            std::format(fmt, std::forward<Args>(args)...).c_str());

        print(line);
    }

    template <typename... ARGS>
    void printf(std::format_string<ARGS...> fmt, ARGS&&... args) {
        print(std::format(fmt, std::forward<ARGS>(args)...));
    }



    template <typename... ARGS>
    void print_log(
        const e_log_level level,
        const std::source_location& location,
        std::format_string<ARGS...> fmt,
        ARGS&&... args) {

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
            std::back_inserter(message), "{}[{:04x}] ", // 끝에 공백
            to_string(level), thread_tag);
        std::format_to(std::back_inserter(message), fmt, std::forward<ARGS>(args)...);
        std::format_to(
            std::back_inserter(message), "\t\t({} ({}))", filename, location.line());
        print(message);
    }

} // namespace ctls


#define ILOG(level, ...)                                                                 \
    ::ctls::print_log((level), std::source_location::current(), __VA_ARGS__)

#ifdef NDEBUG
#define ILOG_TRACE(...) ((void)0)
#define ILOG_DEBUG(...) ((void)0)
#define ILOG_INFO(...) ((void)0)
#else
#define ILOG_TRACE(...) ILOG(::ctls::e_log_level::trace, __VA_ARGS__)
#define ILOG_DEBUG(...) ILOG(::ctls::e_log_level::debug, __VA_ARGS__)
#define ILOG_INFO(...) ILOG(::ctls::e_log_level::info, __VA_ARGS__)
#endif

#define ILOG_WARN(...) ILOG(::ctls::e_log_level::warning, __VA_ARGS__)
#define ILOG_ERROR(...) ILOG(::ctls::e_log_level::error, __VA_ARGS__)
