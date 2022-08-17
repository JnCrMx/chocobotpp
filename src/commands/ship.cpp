#include "colors.h"
#include "command.hpp"

#include <dpp/dpp.h>

#include <limits>
#include <locale>
#include <algorithm>

namespace chocobot {

class ship_command : public command
{
    public:
        ship_command() {}

        std::string get_name() override
        {
            return "ship";
        }

        static constexpr std::array emojis = {":black_heart:", ":heart_decoration:", ":hearts:", ":heart:", ":heartbeat:", ":two_hearts:", ":revolving_hearts:", ":heartpulse:", ":sparkling_heart:", ":gift_heart:"};
        static constexpr auto emoji_null =  ":broken_heart:";
        static constexpr auto emoji_full =  ":cupid:";
        
        bool execute(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t& event, std::istream& args) override
        {
            std::string word1, word2;
            args >> std::quoted(word1) >> std::quoted(word2);
            if(word2 == "x")
            {
                args >> std::quoted(word2);
            }

            dpp::embed embed{};
            embed.set_title(word1+" x "+word2);
            embed.set_color(dpp::colors::magenta);

            std::transform(word1.begin(), word1.end(), word1.begin(), [](char a){return (char)std::tolower(a);});
            std::transform(word2.begin(), word2.end(), word2.begin(), [](char a){return (char)std::tolower(a);});

            char a = std::hash<std::string>()(word1);
            char b = std::hash<std::string>()(word2);

            char r = a ^ b;
            int percent = (r*100)/std::numeric_limits<char>::max();

            std::ostringstream msg;
            msg << std::quoted(word1) << " x " << std::quoted(word2) << ": " << percent << "%";

            std::string emoji;
            if(percent == 0) emoji = emoji_null;
            else if(percent == 100) emoji = emoji_full;
            else emoji = emojis.at((emojis.size() * (percent-1))/100);

            embed.set_description(emoji+" "+std::to_string(percent)+"% "+emoji);

            event.reply(dpp::message(event.msg.channel_id, embed));

            return true;
        }
    private:
        static command_register<ship_command> reg;
};
command_register<ship_command> ship_command::reg{};

}
