#include <iostream>
#include <string>
#include <coroutine>
#include <chrono>

#include <cmath>
#include <date/date.h>
#include <date/tz.h>

import chocobot;
import chocobot.branding;
import chocobot.i18n;
import chocobot.utils;

namespace chocobot {

class daily_command : public command
{
    private:
        std::string get_coins, check_first, check_christmas_gifts, update_coins;
    public:
        daily_command() {}

        static constexpr double log1p_6 = 1.94591014906;
        static constexpr double daily_factor = 30.0 / log1p_6;
        static constexpr double daily_start = 10.0;
        static constexpr int first_bonus = 45;

        static int get_coins_for_streak(int streak)
        {
            return std::log1p(streak) * daily_factor + daily_start;
        }

        std::string get_name() override
        {
            return "daily";
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& connection, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream&) override
        {
			int daily_streak;
            using system_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
            system_time last_daily;

            pqxx::work txn{connection};
            auto result = txn.exec_prepared(get_coins, event.msg.author.id, event.msg.guild_id);
            if(result.empty())
            {
                db.create_user(event.msg.author.id, event.msg.guild_id, txn);
                daily_streak = 0;
                last_daily = system_time(std::chrono::milliseconds(0));
            }
            else
            {
                auto [ld, ds] = result.front().as<unsigned long, int>();
                daily_streak = ds;
                last_daily = system_time(std::chrono::milliseconds(ld));
            }
            auto last_daily_zoned = date::make_zoned(guild.timezone, last_daily);

            auto now = std::chrono::system_clock::now();
            auto now_zoned = date::make_zoned(guild.timezone, now);

            auto today = date::make_zoned(guild.timezone, date::floor<date::days>(now_zoned.get_local_time()));
            result = txn.exec_prepared(check_first, event.msg.guild_id,
                std::chrono::duration_cast<std::chrono::milliseconds>(today.get_sys_time().time_since_epoch()).count());
            bool first = result.empty();

            dpp::embed embed{};
            embed.set_title(i18n::translate(txn, guild, "command.daily.title"));
            embed.set_color(branding::colors::coins);

            std::chrono::days days = std::chrono::duration_cast<std::chrono::days>(last_daily_zoned.get_local_time().time_since_epoch());
            std::chrono::days current_days = std::chrono::duration_cast<std::chrono::days>(now_zoned.get_local_time().time_since_epoch());
            if(days >= current_days)
            {
                event.reply(utils::build_error(txn, guild, "command.daily.error.dup"));
                co_return command::result::user_error;
            }
            if(days+std::chrono::days(1) < current_days)
            {
                daily_streak = 0;
                embed.set_footer(i18n::translate(txn, guild, "command.daily.streak_lost"), {});
            }

            int coins_to_add = get_coins_for_streak(daily_streak) + (first ? first_bonus : 0);
            daily_streak++;

            txn.exec_prepared0(update_coins, event.msg.author.id, event.msg.guild_id, coins_to_add, daily_streak,
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
            int coins = db.get_coins(event.msg.author.id, event.msg.guild_id, txn).value_or(0);

            embed.add_field(i18n::translate(txn, guild, "command.daily.your_coins"), std::to_string(coins));
            embed.add_field(i18n::translate(txn, guild, "command.daily.your_streak"), std::to_string(daily_streak));

            std::ostringstream oss;
            oss << i18n::translate(txn, guild, "command.daily.message", coins_to_add);
            if(first)
            {
                oss << ' ' << i18n::translate(txn, guild, "command.daily.first", first_bonus);
            }
            embed.set_description(oss.str());

            txn.commit();

            event.reply(dpp::message(event.msg.channel_id, embed));

            co_return command::result::success;
        }

        void prepare(chocobot&, database& db) override
        {
            get_coins = db.prepare("get_coins", "SELECT last_daily, daily_streak FROM coins WHERE uid=$1 AND guild=$2");
            check_first = db.prepare("check_first", "SELECT 1 FROM coins WHERE guild=$1 AND last_daily > $2");
            check_christmas_gifts = db.prepare("check_christmas_gifts", "SELECT id FROM christmas_presents WHERE uid=$1 AND guild=$2 AND opened=false");
            update_coins = db.prepare("update_coins", "UPDATE coins SET last_daily=$5, daily_streak=$4, coins=coins+$3 WHERE uid=$1 AND guild=$2");
        }
    private:
        static command_register<daily_command> reg;
};
command_register<daily_command> daily_command::reg{};

}
