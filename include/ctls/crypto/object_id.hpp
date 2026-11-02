#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace ctls {

    class object_id {
    public:
        using bytes_t = std::array<std::uint8_t, 12>;

        object_id() = default;
        explicit object_id(const bytes_t& b) : _bytes{b} {}

        static object_id generate() {
            // (시각 << 24) | 카운터 → 항상 이전 값보다 커지게 CAS로 갱신
            const std::uint64_t now = static_cast<std::uint64_t>(now_seconds()) << 24;
            std::uint64_t prev = state().load(std::memory_order_relaxed);
            std::uint64_t next;
            do {
                next = (prev + 1 > now) ? prev + 1
                                        : now; // 카운터가 넘치면 시각 자리로 올라감
            } while (
                !state().compare_exchange_weak(prev, next, std::memory_order_relaxed));

            const auto ts = static_cast<std::uint32_t>(next >> 24);
            const auto cnt = static_cast<std::uint32_t>(next & 0xFFFFFF);
            const auto& rnd = process_random();

            bytes_t b{};
            b[0] = static_cast<std::uint8_t>(ts >> 24);
            b[1] = static_cast<std::uint8_t>(ts >> 16);
            b[2] = static_cast<std::uint8_t>(ts >> 8);
            b[3] = static_cast<std::uint8_t>(ts);
            for (int i = 0; i < 5; ++i) {
                b[4 + i] = rnd[i];
            }
            b[9] = static_cast<std::uint8_t>(cnt >> 16);
            b[10] = static_cast<std::uint8_t>(cnt >> 8);
            b[11] = static_cast<std::uint8_t>(cnt);
            return object_id{b};
        }

        [[nodiscard]] std::string to_string() const { // 24자리 hex
            static constexpr char hex[] = "0123456789abcdef";
            std::string s(24, '0');
            for (std::size_t i = 0; i < 12; ++i) {
                s[i * 2] = hex[_bytes[i] >> 4];
                s[i * 2 + 1] = hex[_bytes[i] & 0x0F];
            }
            return s;
        }

        static std::optional<object_id> from_string(std::string_view s) {
            if (s.size() != 24) {
                return std::nullopt;
            }
            auto nibble = [](char c) -> int {
                if (c >= '0' && c <= '9') {
                    return c - '0';
                }
                if (c >= 'a' && c <= 'f') {
                    return c - 'a' + 10;
                }
                if (c >= 'A' && c <= 'F') {
                    return c - 'A' + 10;
                }
                return -1;
            };
            bytes_t b{};
            for (std::size_t i = 0; i < 12; ++i) {
                int hi = nibble(s[i * 2]), lo = nibble(s[i * 2 + 1]);
                if (hi < 0 || lo < 0) {
                    return std::nullopt;
                }
                b[i] = static_cast<std::uint8_t>((hi << 4) | lo);
            }
            return object_id{b};
        }

        [[nodiscard]] std::chrono::sys_seconds timestamp() const {
            std::uint32_t ts = (std::uint32_t{_bytes[0]} << 24) |
                               (std::uint32_t{_bytes[1]} << 16) |
                               (std::uint32_t{_bytes[2]} << 8) | std::uint32_t{_bytes[3]};
            return std::chrono::sys_seconds{std::chrono::seconds{ts}};
        }

        [[nodiscard]] const bytes_t& bytes() const {
            return _bytes;
        } // SQLite BLOB 저장용

        auto operator<=>(const object_id&) const = default; // 바이트 순서 = 생성 순서

    private:
        static std::uint32_t now_seconds() {
            using namespace std::chrono;
            return static_cast<std::uint32_t>(
                duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
        }

        static const std::array<std::uint8_t, 5>& process_random() {
            static const auto value = [] {
                std::random_device rd;
                std::array<std::uint8_t, 5> r{};
                for (auto& x : r) {
                    x = static_cast<std::uint8_t>(rd());
                }
                return r;
            }();
            return value;
        }

        static std::atomic<std::uint64_t>& state() {
            static std::atomic<std::uint64_t> s = [] {
                std::random_device rd;
                return (static_cast<std::uint64_t>(now_seconds()) << 24) |
                       (rd() & 0xFFFFFF);
            }();
            return s;
        }

        bytes_t _bytes{};
    };

} // namespace ctls

template <> struct std::hash<ctls::object_id> { // unordered_map 키로 쓸 때
    std::size_t operator()(const ctls::object_id& id) const noexcept {
        std::uint64_t h = 1469598103934665603ULL; // FNV-1a
        for (auto b : id.bytes()) {
            h ^= b;
            h *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(h);
    }
};
