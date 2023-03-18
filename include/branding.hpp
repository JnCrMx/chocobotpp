#pragma once

#include <cstdint>

namespace chocobot::branding {
    namespace colors {
        constexpr uint32_t
            cookie = 253 << 16 | 189 << 8 |  59,
            love   = 255 << 16 |  79 << 8 | 237,
            error  = 255 << 16 |   0 << 8 |   0,
            coins  = 255 << 16 | 255 << 8 |   0,
            warn   = 255 << 16 |  50 << 8 |   0,
            game   = 0   << 16 | 255 << 8 | 229;
    }
    constexpr auto application_name = "ChocoBot";
    constexpr uint64_t ChocoKeks = 443141932714033192UL;
    constexpr auto project_home = "https://git.jcm.re/jcm/chocobotpp";
    constexpr auto author_name = "JCM";
    constexpr auto author_url = "https://git.jcm.re/jcm";
    // TODO: Change once Gitea 1.19 is released and we can use https://git.jcm.re/jcm.png
    constexpr auto author_icon = "https://git.jcm.re/avatars/86ace84d05e13c17d87c9c3debb30ffa";
}
