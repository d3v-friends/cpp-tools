#pragma once
#include <optional>
#include <algorithm>
#include <cctype>
#include <string>

namespace ctls {
    inline std::optional<int> to_int(const std::string& str) {
        std::string digits;
        digits.reserve(str.size());
        for (const char c : str) {
            if ('0' <= c && c <= '9' || c == '-') {
                digits.push_back(c);
            }
        }

        int value = 0;
        const auto* first = digits.data();
        const auto* last = first + digits.size();
        if (const auto [ptr, ec] = std::from_chars(first, last, value);
            ec != std::errc{} || ptr != last) {
            return std::nullopt;
        }
        return value;
    }

    template <class T>
        requires std::integral<T> || std::floating_point<T>
    std::string to_string(const T value) {
        std::array<char, 64> buffer{};
        const auto [ptr, ec] =
            std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (ec != std::errc{}) [[unlikely]] {
            return {};
        }
        return {buffer.data(), ptr};
    }

    inline std::string to_lower(std::string s) { // 값으로 받아서 복사본을 바꿔 반환
        std::ranges::transform(s, s.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

} // namespace ctls
