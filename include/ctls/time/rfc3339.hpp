#pragma once
#include <chrono>
#include <format>
#include <string>
#include <charconv>
#include <optional>
#include <string_view>
#include <ctime>

namespace ctls {
    class rfc3339 {
    public:
        rfc3339()
            : _now{std::chrono::system_clock::now()},
              _string{std::format(
                  "{:%FT%TZ}",
                  std::chrono::floor<std::chrono::milliseconds>(_now))},
              _is_valid{true} {}

        explicit rfc3339(const std::chrono::system_clock::time_point now)
            : _now{now}, _string{std::format(
                             "{:%FT%TZ}",
                             std::chrono::floor<std::chrono::milliseconds>(_now))},
              _is_valid{true} {}

        explicit rfc3339(const std::string_view s) : _is_valid{false} {
            const auto value = parse(s);
            if (!value) {
                return;
            }
            _now = *value;
            _string = std::format(
                "{:%FT%TZ}", std::chrono::floor<std::chrono::milliseconds>(_now));
            _is_valid = true;
        }

        ~rfc3339() = default;
        rfc3339(const rfc3339&) = default;
        rfc3339& operator=(const rfc3339&) = default;
        rfc3339(rfc3339&&) = default;
        rfc3339& operator=(rfc3339&&) = default;

        // 함수를 실행한 시각 기준으로 남은 분
        // 양수: 기다려야 함, 0 이하: 이미 지남, nullopt: 무효 객체
        [[nodiscard]] static std::optional<long long> diff_minutes(const rfc3339& time) {
            using namespace std::chrono;
            if (!time._is_valid) {
                return std::nullopt;
            }
            return ceil<minutes>(time._now - system_clock::now()).count();
        }

        // 함수를 실행한 시각 기준으로 남은 초
        // 양수: 기다려야 함, 0 이하: 이미 지남, nullopt: 무효 객체
        [[nodiscard]] static std::optional<long long> diff_seconds(const rfc3339& time) {
            using namespace std::chrono;
            if (!time._is_valid) {
                return std::nullopt;
            }
            return ceil<seconds>(time._now - system_clock::now()).count();
        }

        // 함수를 실행한 시각 기준으로 남은 일수
        // 양수: 기다려야 함, 0 이하: 이미 지남, nullopt: 무효 객체
        [[nodiscard]] static std::optional<long long> diff_days(const rfc3339& time) {
            using namespace std::chrono;
            if (!time._is_valid) {
                return std::nullopt;
            }
            return ceil<days>(time._now - system_clock::now()).count();
        }

        [[nodiscard]] std::string to_string() const {
            if (!_is_valid) {
                return "";
            }
            return _string;
        }

        [[nodiscard]] std::optional<std::string> local_string() const {
            using namespace std::chrono;
            if (!_is_valid) {
                return std::nullopt;
            }
            const auto sec = floor<seconds>(_now); // ms 버림
            const auto off = utc_offset(system_clock::to_time_t(sec));
            if (!off) {
                return std::nullopt;
            }
            const local_seconds local{sec.time_since_epoch() + *off};
            return std::format("{:%FT%T}", local); // 초 단위라 소수부 안 붙음
        }

        static rfc3339 from_string(const std::string& str) { return rfc3339{str}; }

        [[nodiscard]] std::optional<std::string> string() const {
            if (!_is_valid) {
                return std::nullopt;
            }
            return _string;
        }

        [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
        time_point() const {
            if (!_is_valid) {
                return std::nullopt;
            }
            return _now;
        }

    private:
        std::chrono::system_clock::time_point _now;
        std::string _string;
        bool _is_valid;

        [[nodiscard]] static std::optional<std::chrono::seconds>
        utc_offset(const std::time_t t) noexcept {
            std::tm tm{};
#if defined(_WIN32)
            if (localtime_s(&tm, &t) != 0) {
                return std::nullopt;
            }
            std::tm gm{};
            if (gmtime_s(&gm, &t) != 0) {
                return std::nullopt;
            }
            gm.tm_isdst = tm.tm_isdst; // mktime 이 DST 보정 안 하게
            return std::chrono::seconds{static_cast<long long>(t - std::mktime(&gm))};
#else
            if (localtime_r(&t, &tm) == nullptr) {
                return std::nullopt;
            }
            return std::chrono::seconds{tm.tm_gmtoff};
#endif
        }

        [[nodiscard]] static std::optional<std::chrono::system_clock::time_point>
        parse(const std::string_view s) noexcept {
            using namespace std::chrono;
            const char* p = s.data();
            const char* const end = p + s.size();
            // 정확히 width 자리 숫자 읽기
            auto num = [&](const unsigned int width, int& out) -> bool {
                if (end - p < width) {
                    return false;
                }
                auto [ptr, ec] = std::from_chars(p, p + width, out);
                if (ec != std::errc{} || ptr != p + width || out < 0) {
                    return false; // "-1" 차단
                }
                p = ptr;
                return true;
            };
            // 구분자 확인 (RFC 3339는 'T'/'Z' 소문자도 허용)
            auto expect = [&](char upper, char lower) -> bool {
                if (p == end || (*p != upper && *p != lower)) {
                    return false;
                }
                ++p;
                return true;
            };
            int Y{}, M{}, D{}, h{}, m{}, sec{};
            const bool ok = num(4, Y) && expect('-', '-') && num(2, M) &&
                            expect('-', '-') && num(2, D) && expect('T', 't') &&
                            num(2, h) && expect(':', ':') && num(2, m) &&
                            expect(':', ':') && num(2, sec);
            if (!ok) {
                return std::nullopt;
            }

            // 소수부: 앞 3자리만 ms로 사용, 나머지는 버림
            int ms = 0;
            if (p != end && *p == '.') {
                ++p;
                int digits = 0;
                while (p != end && *p >= '0' && *p <= '9') {
                    if (digits < 3) {
                        ms = ms * 10 + (*p - '0');
                    }
                    ++digits;
                    ++p;
                }
                if (digits == 0) {
                    return std::nullopt; // "12." 처럼 빈 소수부
                }
                for (int i = digits; i < 3; ++i) {
                    ms *= 10; // ".5" → 500ms
                }
            }
            if (!expect('Z', 'z') || p != end) {
                return std::nullopt; // +09:00 오프셋은 미지원
            }
            const year_month_day ymd{
                year{Y}, month{static_cast<unsigned>(M)}, day{static_cast<unsigned>(D)}};
            if (!ymd.ok() || h > 23 || m > 59 || sec > 59) {
                return std::nullopt;
            }
            return sys_days{ymd} + hours{h} + minutes{m} + seconds{sec} +
                   milliseconds{ms};
        }
    };

} // namespace ctls
