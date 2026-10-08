#pragma once
#include <string_view>
#include <ctime>
#include <format>
#include <chrono>

namespace ctls {
    inline void print(std::string_view str) {
        const auto now =
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local{};
        localtime_s(&local, &now);

        char time_text[32];
        std::strftime(time_text, sizeof(time_text), "[%Y-%m-%d][%H:%M:%S]", &local);

        const auto line = std::format("{} {}\n", time_text, str);
        std::fputs(line.c_str(), stderr);
    }
} // namespace ctls