module;

#include <cstdint>
#include <unordered_map>
#include <string_view>
#include <array>

export module chocobot.branding;

export namespace chocobot::branding {
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
    constexpr auto author_icon = "https://git.jcm.re/jcm.png";

    constexpr auto default_tax_rate = 0.05;

    std::unordered_map<std::string_view, std::string_view> translators = {
        {{"pl", "Cyndi"}}
    };
    constexpr std::array answer_emojis = {"1️⃣", "2️⃣", "3️⃣", "4️⃣", "5️⃣", "6️⃣", "7️⃣", "8️⃣", "9️⃣"};
    constexpr auto confirm_emoji = "✅";
    constexpr auto cancel_emoji = "❌";
}
