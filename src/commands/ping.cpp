#include "command.hpp"

#include <dpp/dpp.h>

namespace chocobot {

class ping_command : public command
{
    public:
        ping_command() {}

        std::string get_name() override
        {
            return "ping";
        }

        result execute(chocobot&, pqxx::connection&, database&, dpp::cluster& discord, const guild&, const dpp::message_create_t& event, std::istream&) override
        {
            discord.message_create(dpp::message(event.msg.channel_id, "Pong!").set_reference(event.msg.id));
            return command::result::success;
        }
    private:
        static command_register<ping_command> reg;
};
command_register<ping_command> ping_command::reg{};

}
