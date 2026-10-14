#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace ctls {
    inline std::vector<std::string> str_split(const std::string& s, const char delim) {
        std::vector<std::string> out;
        std::istringstream iss(s);
        std::string token;
        while (std::getline(iss, token, delim)) {
            out.push_back(token);
        }

        if (s.empty() || s.back() == delim) {
            out.emplace_back();
        }
        return out;
    }

} // namespace ctls
