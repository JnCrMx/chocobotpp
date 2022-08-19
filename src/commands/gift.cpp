#include "command.hpp"
#include "chocobot.hpp"
#include "i18n.hpp"
#include "branding.hpp"
#include "utils.hpp"

#include <dpp/dpp.h>

namespace chocobot {

class gift_command : public command
{
    public:
        gift_command() {}

        std::string get_name() override
        {
            return "gift";
        }

        result execute(chocobot&, pqxx::connection& connection, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream& args) override
        {
			std::string receiver;
			args >> receiver;
			std::optional<dpp::snowflake> recvo = utils::parse_mention(receiver);
			if(!recvo.has_value())
			{
				event.reply(utils::build_error(connection, guild, "command.gift.error.who"));
				return command::result::user_error;
			}
			int amount;
			args >> amount;
			if(amount < 0)
			{
				event.reply(utils::build_error(connection, guild, "command.gift.error.negative"));
				return command::result::user_error;
			}
			if(amount == 0)
			{
				event.reply(utils::build_error(connection, guild, "command.gift.error.zero"));
				return command::result::user_error;
			}

			auto recv = utils::provide_user(discord, recvo.value());
			{
				pqxx::work txn(connection);
				if(db.get_coins(event.msg.author.id, guild.id, txn) < amount)
				{
					event.reply(utils::build_error(txn, guild, "command.gift.error.not_enough"));
					return command::result::user_error;
				}
				
				if(!recv.is_bot())
				{
					db.change_coins(event.msg.author.id, guild.id, -amount, txn);
					db.change_coins(recv.id, guild.id, amount, txn);
					txn.commit();
				}
			}

			dpp::embed embed{};
			embed.set_title(i18n::translate(connection, guild, "command.gift.title"));
			embed.set_color(branding::colors::coins);
			embed.set_description(i18n::translate(connection, guild, amount==1?"command.gift.message.one":"command.gift.message.many",
				event.msg.author.format_username(), recv.format_username(), amount));
			if(recv.id == discord.me.id)
			{
				embed.set_footer(i18n::translate(connection, guild, "command.gift.thankyou"), {});
			}
			event.reply(dpp::message({}, embed));

			return command::result::success;
		}
    private:
        static command_register<gift_command> reg;
};
command_register<gift_command> gift_command::reg{};

}
