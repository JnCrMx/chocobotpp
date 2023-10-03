#include "command.hpp"

namespace chocobot {

class ping_command : public command
{
    public:
        ping_command() {}

        std::string get_name() override
        {
            return "ping";
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection&, database&, dpp::cluster& discord, const guild&, const dpp::message_create_t& event, std::istream& args) override
        {
            int delay = 0;
            args >> delay;
            co_await discord.co_sleep(delay);
            event.reply("Pong in "+std::to_string((int)(discord.rest_ping*1000.0))+" ms!");
            co_return command::result::success;
        }
    private:
        static command_register<ping_command> reg;
};
command_register<ping_command> ping_command::reg{};

}
