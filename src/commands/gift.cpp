#include "command.hpp"
#include "chocobot.hpp"
#include "i18n.hpp"
#include "branding.hpp"
#include "utils.hpp"

namespace chocobot {

class gift_command : public command
{
    public:
        gift_command() {}

        std::string get_name() override
        {
            return "gift";
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& connection, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream& args) override
        {
			std::string receiver;
			args >> receiver;
			std::optional<dpp::snowflake> recvo = utils::parse_mention(receiver);
			if(!recvo.has_value())
			{
				event.reply(utils::build_error(connection, guild, "command.gift.error.who"));
				co_return command::result::user_error;
			}
			int amount;
			args >> amount;
			if(amount < 0)
			{
				event.reply(utils::build_error(connection, guild, "command.gift.error.negative"));
				co_return command::result::user_error;
			}
			if(amount == 0)
			{
				event.reply(utils::build_error(connection, guild, "command.gift.error.zero"));
				co_return command::result::user_error;
			}

			auto recv = (co_await discord.co_user_get_cached(recvo.value())).get<dpp::user_identified>();
			{
				pqxx::work txn(connection);
				if(db.get_coins(event.msg.author.id, guild.id, txn) < amount)
				{
					event.reply(utils::build_error(txn, guild, "command.gift.error.not_enough"));
					co_return command::result::user_error;
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

			co_return command::result::success;
		}
    private:
        static command_register<gift_command> reg;
};
command_register<gift_command> gift_command::reg{};

}
