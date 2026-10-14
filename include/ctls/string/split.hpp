#pragma once

#include <string>
#include <vector>

namespace ctls {

    inline std::vector<std::string>
    str_split(std::string_view s, const std::string_view delim) {
        std::vector<std::string> out;

        if (delim.empty()) {
            out.emplace_back(s); // push_back → emplace_back
            return out;
        }

        size_t start = 0;
        size_t pos;
        while ((pos = s.find(delim, start)) != std::string_view::npos) {
            out.emplace_back(s.substr(start, pos - start)); // push_back → emplace_back
            start = pos + delim.size();
        }
        out.emplace_back(s.substr(start)); // push_back → emplace_back

        return out;
    }

} // namespace ctls
