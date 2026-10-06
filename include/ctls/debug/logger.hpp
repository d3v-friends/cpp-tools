#pragma once
#include <format>
#include <source_location>
#include <string_view>
#include <thread>

// platform 별 헤더 불러오기
#if defined(_WIN32)
#include "ctls/logger/windows.hpp"
#elif defined(__APPLE__)
#include "ctls/logger/mac_linux.hpp"
#elif defined(__linux__)
#include "ctls/logger/mac_linux.hpp"
#endif

namespace ctls::logger {
    inline void print_line(const std::source_location& loc, std::string_view str) {
        const auto line = std::format("[{}:{}] {}\n", loc.file_name(), loc.line(), str);
        logger::print(line);
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

        logger::print(line);
    }

    template <typename... ARGS>
    void printf(std::format_string<ARGS...> fmt, ARGS&&... args) {
        logger::print(std::format(fmt, std::forward<ARGS>(args)...));
    }
} // namespace ctls::logger

#ifdef NDEBUG
#define I_LOG(str) ((void)0)
#define I_LOG_F(fmt, ...) ((void)0)
#else
#define I_LOG(str) ctls::logger::print_line(std::source_location::current(), str)
#define I_LOG_F(fmt, ...)                                                                \
    ctls::logger::printf_line(std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
#endif