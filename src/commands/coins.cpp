#include "command.hpp"
#include "chocobot.hpp"
#include "i18n.hpp"

#include <dpp/dpp.h>

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

        bool execute(chocobot&, pqxx::connection& connection, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream&) override
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

            std::chrono::days days = std::chrono::duration_cast<std::chrono::days>(last_daily.time_since_epoch());
            std::chrono::days current_days = std::chrono::duration_cast<std::chrono::days>(std::chrono::system_clock::now().time_since_epoch());

            dpp::embed embed{};
            embed.set_title(translate(connection, guild, "command.coins.title"));
            embed.set_color(dpp::colors::orange);
            embed.add_field(translate(connection, guild, "command.coins.your"), std::to_string(coins));
            if(days < current_days)
            {
                embed.set_footer(dpp::embed_footer().set_text(translate(connection, guild, "command.coins.daily")));
            }
            event.reply(dpp::message(event.msg.channel_id, embed));

            return true;
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
