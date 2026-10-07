#pragma once
#include <string>

namespace ctls {
    inline std::string get_env(const std::string& key) {
        const char* ptr = std::getenv(key.c_str());
        if (ptr == nullptr) {
            return "";
        }
        return ptr;
    }
} // namespace ctls
