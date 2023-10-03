#include "command.hpp"

#include <utils.hpp>
#include <i18n.hpp>
#include <branding.hpp>

namespace chocobot {

class help_command : public command
{
    public:
        help_command() {}

        std::string get_name() override
        {
            return "help";
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& conn, database& db,
            dpp::cluster&, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            std::string command;
            args >> command;

            pqxx::nontransaction txn{conn};

            const auto& map = *command_factory::get_map();
            if(command.empty())
            {
                dpp::embed eb{};
                eb.set_title(i18n::translate(txn, guild, "command.help.title"));
                eb.set_color(branding::colors::cookie);
                for(const auto& [key, _] : map)
                {
                    eb.add_field(guild.prefix + key, i18n::translate(txn, guild, "command."+key+".help"));
                }
                event.reply(dpp::message{dpp::snowflake{}, eb});

                co_return result::success;
            }
            else
            {
                if(command.starts_with(guild.prefix))
                {
                    command = command.substr(guild.prefix.size());
                }
                if(!map.contains(command))
                {
                    event.reply(utils::build_error(txn, guild, "command.help.error.noent"));
                    co_return result::user_error;
                }
                auto helpText = i18n::translate(txn, guild, "command."+command+".help");
                event.reply(i18n::translate(txn, guild, "command."+command+".usage",
                    fmt::arg("prefix", guild.prefix),
                    fmt::arg("cmd", guild.prefix+command),
                    fmt::arg("command", command),
                    fmt::arg("help", helpText)));

                co_return result::success;
            }
        }
    private:
        static command_register<help_command> reg;
};
command_register<help_command> help_command::reg{};

}
