#include <iostream>
#include <string>
#include <coroutine>

#include <date/date.h>
#include <date/tz.h>

import chocobot;
import chocobot.branding;
import chocobot.i18n;
import pqxx;

namespace chocobot {

class coins_command : public command
{
    private:
        std::string statement;
    public:
        coins_command() {}

        std::string get_name() override
        {
            return "coins";
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& connection, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream&) override
        {
            int coins;
            using system_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
            system_time last_daily;

            pqxx::nontransaction txn{connection};
            auto result = txn.exec_prepared(statement, event.msg.author.id, event.msg.guild_id);
            if(result.empty())
            {
                db.create_user(event.msg.author.id, event.msg.guild_id, txn);
                coins = 0;
                last_daily = system_time(std::chrono::milliseconds(0));
            }
            else
            {
                auto [c, ld] = result.front().as<int, unsigned long>();
                coins = c;
                last_daily = system_time(std::chrono::milliseconds(ld));
            }
            auto last_daily_zoned = date::make_zoned(guild.timezone, last_daily);
            auto now_zoned = date::make_zoned(guild.timezone, std::chrono::system_clock::now());

            std::chrono::days days = std::chrono::duration_cast<std::chrono::days>(last_daily_zoned.get_local_time().time_since_epoch());
            std::chrono::days current_days = std::chrono::duration_cast<std::chrono::days>(now_zoned.get_local_time().time_since_epoch());

            dpp::embed embed{};
            embed.set_title(i18n::translate(txn, guild, "command.coins.title"));
            embed.set_color(branding::colors::coins);
            embed.add_field(i18n::translate(txn, guild, "command.coins.your"), std::to_string(coins));
            if(days < current_days)
            {
                embed.set_footer(dpp::embed_footer().set_text(i18n::translate(txn, guild, "command.coins.daily")));
            }
            event.reply(dpp::message(event.msg.channel_id, embed));

            co_return command::result::success;
        }

        void prepare(chocobot&, database& db) override
        {
            statement = db.prepare("get_coins", "SELECT coins, last_daily FROM coins WHERE uid=$1 AND guild=$2");
        }
    private:
        static command_register<coins_command> reg;
};
command_register<coins_command> coins_command::reg{};

}
