#include <iostream>
#include <string>
#include <coroutine>

import chocobot;
import chocobot.branding;
import chocobot.i18n;
import chocobot.utils;
import pqxx;

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
                for(const auto& [key, command] : map)
                {
                    std::string description = i18n::translate(txn, guild, "command."+key+".help");
                    if(paid_command* paid = dynamic_cast<paid_command*>(command.get())) {
                        description = description+" "+i18n::translate(txn, guild, "command.paid.help_cost", paid->get_cost());
                    }
                    eb.add_field(guild.prefix + key, description);
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
                if(paid_command* paid = dynamic_cast<paid_command*>(map.at(command).get())) {
                    helpText = helpText+" "+i18n::translate(txn, guild, "command.paid.help_cost", paid->get_cost());
                }
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
