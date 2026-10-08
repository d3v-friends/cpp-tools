#pragma once
#include <optional>
namespace ctls {
    inline std::optional<int> to_int(const std::string& str) {
        std::string digits;
        digits.reserve(str.size());
        for (const char c : str) {
            if ('0' <= c && c <= '9') {
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

} // namespace ctls
