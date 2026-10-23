#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ctls {
    inline std::string get_absolute_filepath(const std::string& filename) {
        std::filesystem::path path = filename;

        if (!std::filesystem::path(filename).is_absolute()) {
            path = std::filesystem::absolute(filename);
        }

        return path.lexically_normal().string();
    }

    // 윈도우용 한글경로 utf-8 변경
    inline std::string to_utf8(const std::filesystem::path& path) {
        const auto u8 = path.generic_u8string();
        return {reinterpret_cast<const char*>(u8.data()), u8.size()};
    }
} // namespace ctls
