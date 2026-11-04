#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <vector>
#endif

#if defined(_WIN32)
// 반드시 전역 네임스페이스에 둬야 windows.h의 HINSTANCE__와 같은 타입이 됨
struct HINSTANCE__;

extern "C" __declspec(dllimport)
unsigned long __stdcall GetModuleFileNameW(HINSTANCE__* hModule,
                                           wchar_t* lpFilename,
                                           unsigned long nSize);
#endif

namespace ctls {
    namespace detail {
        // UTF-8 std::string → path (Windows 한글 경로 대응)
        inline std::filesystem::path from_utf8(std::string_view s) {
#if defined(__cpp_char8_t)
            return std::filesystem::path(
                std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
#else
            return std::filesystem::u8path(s);
#endif
        }

        inline std::filesystem::path executable_path() {
#if defined(_WIN32)
            std::wstring buf(260, L'\0'); // 260 == MAX_PATH
            for (;;) {
                unsigned long n = ::GetModuleFileNameW(
                    nullptr, buf.data(), static_cast<unsigned long>(buf.size()));
                if (n == 0) {
                    return {};
                }
                if (n < buf.size()) {
                    buf.resize(n);
                    break;
                }
                buf.resize(buf.size() * 2); // MAX_PATH보다 긴 경로 대응
            }
            return std::filesystem::path(buf);
#elif defined(__APPLE__)
            std::uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size); // 필요한 크기 먼저 받기
            std::vector<char> buf(size);
            if (_NSGetExecutablePath(buf.data(), &size) != 0) {
                return {};
            }
            std::error_code ec;
            auto p = std::filesystem::canonical(buf.data(), ec); // 심볼릭 링크, ../ 정리
            return ec ? std::filesystem::path(buf.data()) : p;
#elif defined(__linux__)
            std::error_code ec;
            return std::filesystem::read_symlink("/proc/self/exe", ec);
#else
            return {};
#endif
        }
    } // namespace detail

    // 윈도우용 한글경로 utf-8 변경
    inline std::string to_utf8(const std::filesystem::path& path) {
        const auto u8 = path.generic_u8string();
        return {reinterpret_cast<const char*>(u8.data()), u8.size()};
    }

    // 입력/출력 모두 UTF-8
    inline std::string get_absolute_filepath(const std::string& filename) {
        return to_utf8(std::filesystem::absolute(detail::from_utf8(filename)).lexically_normal());
    }

    // 실행 파일이 있는 폴더 (SDL_GetBasePath 대응, 끝에 구분자 없음)
    inline const std::filesystem::path& base_path() {
        static const std::filesystem::path dir = detail::executable_path().parent_path();
        return dir;
    }

    inline std::filesystem::path from_base_path(const std::filesystem::path& path) {
        return (base_path() / path).lexically_normal();
    }
} // namespace ctls