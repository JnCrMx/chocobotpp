#include "command.hpp"

#include <dpp/dpp.h>

namespace chocobot {

class coins_command : public command
{
    public:
        coins_command() {}

        std::string get_name() override
        {
            return "coins";
        }

        bool execute(chocobot&, dpp::cluster& discord, const guild&, const dpp::message_create_t& event, std::istream&) override
        {
            discord.message_create(dpp::message(event.msg.channel_id, "Pong!").set_reference(event.msg.id));
            return true;
        }
    private:
        static command_register<coins_command> reg;
};
command_register<coins_command> coins_command::reg{};

}
