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

        dpp::coroutine<result> execute(chocobot& bot, pqxx::connection& conn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            dpp::embed eb{};
            eb.set_title(i18n::translate(conn, guild, "command.credits.title", branding::application_name));
            eb.set_url(branding::project_home);
            eb.set_author(branding::author_name, branding::author_url, branding::author_icon);
            eb.set_color(branding::colors::cookie);
            eb.add_field(i18n::translate(conn, guild, "command.credits.dedication"),
                (co_await discord.co_user_get_cached(branding::ChocoKeks)).get<dpp::user_identified>().format_username());
            eb.add_field(i18n::translate(conn, guild, "command.credits.event"), "Weihnachten 2019");
            eb.add_field(i18n::translate(conn, guild, "command.credits.version"), std::string{git::Describe()});
            for(const auto& [lang, translator] : branding::translators) {
                eb.add_field(
                    i18n::translate(conn, guild, "command.credits.translator."+std::string(lang)),
                    std::string(translator)
                );
            }
            event.reply(dpp::message{dpp::snowflake{}, eb});

            co_return result::success;
        }
    private:
        static command_register<credits_command> reg;
};
command_register<credits_command> credits_command::reg{};

}
