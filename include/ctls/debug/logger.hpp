#pragma once
#include <format>
#include <source_location>
#include <string_view>
#include <thread>

#include "ctls/logger/logger.hpp"

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
} // namespace ctls

#ifdef NDEBUG
#define I_LOG(str) ((void)0)
#define I_LOG_F(fmt, ...) ((void)0)
#else
#define I_LOG(str) ctls::print_line(std::source_location::current(), str)
#define I_LOG_F(fmt, ...)                                                                \
    ctls::printf_line(std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
#endif