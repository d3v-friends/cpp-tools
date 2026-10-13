#pragma once
#include <string_view>
#include <ctime>
#include <format>
#include <chrono>

namespace ctls {
    inline void print(std::string_view str) {
        try {
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

            char time_text[32];
            const auto time_len =
                std::strftime(time_text, sizeof(time_text), "%Y-%m-%d][%H:%M:%S", &local);

            std::string line;
            line.reserve(time_len + str.size() + 8); // "[" ".mmm]" "\n"
            std::format_to(
                std::back_inserter(line), "[{}.{:03}]{}\n",
                std::string_view{time_text, time_len}, ms, str);

            // 한 번의 호출로 한 줄 전체를 씀 → 스레드 간 줄 섞임 없음
            // stderr는 unbuffered → assert/abort 직전 로그도 나옴
            std::fwrite(line.data(), 1, line.size(), stderr);
        } catch (...) {
            // 로깅 실패가 프로그램을 죽이지 않게 삼킴 (할당 없는 경로로 흔적만 남김)
            std::fputs("[log] print failed\n", stderr);
        }
    }
} // namespace ctls