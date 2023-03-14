#include "command.hpp"
#include "i18n.hpp"
#include "branding.hpp"
#include "utils.hpp"

#include "git.h"

namespace chocobot {

class credits_command : public command
{
    public:
        credits_command() {}

        std::string get_name() override
        {
            return "credits";
        }

        result execute(chocobot& bot, pqxx::connection& conn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            dpp::embed eb{};
            eb.set_title(i18n::translate(conn, guild, "command.credits.title"));
            eb.set_color(branding::colors::cookie);
            eb.add_field(i18n::translate(conn, guild, "command.credits.dedication"),
                utils::provide_user(discord, branding::ChocoKeks).format_username());
            eb.add_field(i18n::translate(conn, guild, "command.credits.event"), "Weihnachten 2019");
            eb.add_field(i18n::translate(conn, guild, "command.credits.version"), std::string{git::Describe()});
            event.reply(dpp::message{dpp::snowflake{}, eb});

            return result::success;
        }
    private:
        static command_register<credits_command> reg;
};
command_register<credits_command> credits_command::reg{};

}