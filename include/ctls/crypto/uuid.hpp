#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/random.h> // getentropy (macOS 10.12+, glibc 2.25+)
#endif

namespace ctls {
    // OS 의 암호학적 난수로 buffer 를 채운다. 실패하면 false
    inline bool fill_secure_random(std::uint8_t* buffer, const std::size_t size) {
#if defined(_WIN32)
        return BCRYPT_SUCCESS(BCryptGenRandom(
            nullptr, buffer, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
#else
        // getentropy 는 한 번에 최대 256 바이트
        return size <= 256 && getentropy(buffer, size) == 0;
#endif
    }

    // 안전한 UUID v4 문자열 생성. 난수 생성에 실패하면 std::nullopt
    inline std::optional<std::string> make_secure_uuid_v4() {
        std::array<std::uint8_t, 16> bytes{};
        if (!fill_secure_random(bytes.data(), bytes.size())) [[unlikely]] {
            return std::nullopt;
        }

        bytes[6] = (bytes[6] & 0x0F) | 0x40; // 버전 4
        bytes[8] = (bytes[8] & 0x3F) | 0x80; // variant (RFC 4122)

        std::string out;
        out.reserve(36);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                out.push_back('-');
            }
            out += std::format("{:02x}", bytes[i]);
        }
        return out;
    }
} // namespace ctls
