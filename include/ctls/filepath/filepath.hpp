#pragma once

#include <filesystem>
#include <string>

namespace ctls {
    inline std::string get_absolute_filepath(const std::string& filename) {
        std::filesystem::path path = filename;

        if (!std::filesystem::path(filename).is_absolute()) {
            path = std::filesystem::absolute(filename);
        }

        return path.lexically_normal().string();
    }
} // namespace ctls
