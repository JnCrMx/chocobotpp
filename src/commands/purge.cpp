#include <iostream>
#include <string>
#include <coroutine>
#include <vector>

import chocobot;
import chocobot.branding;
import chocobot.i18n;
import chocobot.utils;

namespace chocobot {

class purge_command : public command
{
    public:
        purge_command() {}

        std::string get_name() override
        {
            return "purge";
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& connection, database&, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream& args) override
        {
            std::string message1, message2;
            if(!(args >> message1 >> message2))
            {
                event.reply(utils::build_error(connection, guild, "command.purge.error.nargs"));
                co_return command::result::user_error;
            }

            dpp::snowflake guild1, guild2, channel1, channel2, message1_id, message2_id;
            if(!utils::parse_message_link(message1, guild1, channel1, message1_id)) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.invalid_message_link"));
                co_return command::result::user_error;
            }
            if(!utils::parse_message_link(message2, guild2, channel2, message2_id)) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.invalid_message_link"));
                co_return command::result::user_error;
            }
            if(guild1 != guild.id || guild2 != guild.id) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.wrong_guild"));
                co_return command::result::user_error;
            }
            if(channel1 != channel2) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.two_channels"));
                co_return command::result::user_error;
            }
            if(message1_id > message2_id) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.invalid_order"));
                co_return command::result::user_error;
            }
            const auto& channel = channel1;

            dpp::snowflake first = message1_id-1;
            dpp::snowflake last = message2_id+1;

            dpp::snowflake user = event.msg.author.id;

            std::vector<dpp::snowflake> messages{};
            do {
                auto res = co_await discord.co_messages_get(channel, 0, last, first, 100);
                if(res.is_error()) {
                    continue;
                }
                const auto& m = res.get<dpp::message_map>();
                for(const auto& [id, message] : m) {
                    if(message.author.id == user && id >= message1_id && id <= message2_id)
                        messages.push_back(id);
                    first = std::max(first, id);
                }
            }
            while(first < last);

            if(messages.empty()) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.no_messages"));
                co_return command::result::user_error;
            }

            dpp::embed embed{};
            embed.set_title(i18n::translate(connection, guild, "command.purge.confirm.title", messages.size()));
            embed.set_color(branding::colors::warn);
            embed.set_description(i18n::translate(connection, guild, "command.purge.confirm.description", messages.size()));
            embed.set_footer(i18n::translate(connection, guild, "command.purge.confirm.footer"), {});
            auto msg_id = (co_await discord.co_message_create(dpp::message{channel, embed})).get<dpp::message>().id;
            discord.message_add_reaction(msg_id, channel, branding::confirm_emoji);
            discord.message_add_reaction(msg_id, channel, branding::cancel_emoji);
            auto result = co_await dpp::when_any{
                discord.on_message_reaction_add.when([&](const dpp::message_reaction_add_t& e)->bool {
                    if(e.message_id != msg_id) return false;
                    dpp::snowflake reacting_user = nlohmann::json::parse(e.raw_event)["d"]["user_id"]; // for some reason dpp does not extract this field correctly... :/
                    if(reacting_user != user) return false;

                    if(e.reacting_emoji.name != branding::confirm_emoji
                        && e.reacting_emoji.name != branding::cancel_emoji) return false;

                    return true;
                }),
                event.from->creator->co_sleep(60)
            };
            discord.message_delete(msg_id, channel);
            if(result.index() == 1 || result.get<0>().reacting_emoji.name != branding::confirm_emoji) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.cancel"));
                co_return command::result::user_error;
            }
            auto res = co_await discord.co_message_delete_bulk(messages, channel);
            if(res.is_error()) {
                event.reply(utils::build_error(connection, guild, "command.purge.error.failed"));
                spdlog::error("Failed to delete messages: {}", res.get_error().message);
                co_return command::result::system_error;
            }

            dpp::embed eb{};
            eb.set_color(branding::colors::cookie);
            eb.set_title(i18n::translate(connection, guild, "command.purge.success.title", messages.size()));
            eb.set_description(i18n::translate(connection, guild, "command.purge.success.description", messages.size()));
            event.reply(dpp::message({}, eb));

            co_return command::result::success;
        }
    private:
        static command_register<purge_command> reg;
};
command_register<purge_command> purge_command::reg{};

}
