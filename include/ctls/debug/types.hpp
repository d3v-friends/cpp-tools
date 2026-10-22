#pragma once
#include <string_view>
#include <cstdint>

namespace ctls {
    enum class e_log_level : std::uint8_t {
        trace = 0,
        debug = 1,
        info = 2,
        warning = 3,
        error = 4,
    };

    inline std::string_view to_string(const e_log_level level) {
        switch (level) {
        case e_log_level::trace:
            return "[TRACE]";
        case e_log_level::debug:
            return "[DEBUG]";
        case e_log_level::info:
            return "[INFO_]";
        case e_log_level::warning:
            return "[WARN_]";
        case e_log_level::error:
            return "[ERROR]";
        default:
            return "[_____]";
        }
    }

    inline std::string_view to_color(const e_log_level level) {
        switch (level) {
        case e_log_level::trace:
            return "\033[90m";
        case e_log_level::debug:
            return "\033[36m";
        case e_log_level::info:
            return "\033[32m";
        case e_log_level::warning:
            return "\033[33m";
        case e_log_level::error:
            return "\033[31m";
        default:
            return "";
        }
    }

    inline constexpr std::string_view k_color_reset = "\033[0m";
} // namespace ctls