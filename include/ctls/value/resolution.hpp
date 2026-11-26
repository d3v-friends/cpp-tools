#pragma once

namespace ctls {
    struct ivec2 {
        int x;
        int y;
    };

    inline constexpr ivec2 HD{
        .x = 1280,
        .y = 720,
    };
    inline constexpr ivec2 FHD{
        .x = 1920,
        .y = 1080,
    };
    inline constexpr ivec2 QHD{
        .x = 2560,
        .y = 1440,
    };
    inline constexpr ivec2 UHD{
        .x = 3840,
        .y = 2160,
    };
} // namespace ctls