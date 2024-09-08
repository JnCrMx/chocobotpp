module;

#include <random>
#include <iostream>
#include <string>
#include <coroutine>
#include <unordered_map>

module chocobot;

import chocobot.branding;
import chocobot.i18n;
import chocobot.utils;
import chocobot.config;
import pqxx;
import dpp;

namespace chocobot {

static int get_tax(int cost, double tax_rate)
{
    double tax = cost * tax_rate;
    long int_tax = static_cast<long>(tax);
    double fract = tax - int_tax;
    if(fract == 0.0) {
        return int_tax;
    }
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<> dist(0.0, 1.0);
    return dist(rng) < fract ? int_tax : (int_tax + 1);
}

dpp::coroutine<bool> paid_command::preflight(chocobot& bot, pqxx::connection& conn, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream&)
{
    dpp::snowflake user_id = event.msg.author.id;
    int cost = get_cost();

    pqxx::work txn{conn};
    if(db.get_coins(user_id, guild.id, txn).value_or(0) < cost) {
        dpp::embed embed{};
        embed.set_title(i18n::translate(txn, guild, "error"));
        embed.set_color(branding::colors::error);
        embed.set_description(i18n::translate(txn, guild, "command.paid.error.not_enough", cost));
        event.reply(dpp::message({}, embed));
        co_return false;
    }

    db.change_coins(user_id, guild.id, -cost, txn);
    txn.commit();

    co_return true;
}

dpp::coroutine<> paid_command::postexecute(chocobot& bot, pqxx::connection& conn, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream&, result result)
{
    dpp::snowflake user_id = event.msg.author.id;
    int cost = get_cost();

    pqxx::work txn{conn};
    if(result == command::result::success)
    {
        // only pay taxes if command succeeds
        const auto& taxes = bot.cfg().taxes;
        const auto& name = get_name();
        if(taxes.contains(name)) {
            const auto& [receiver, rate] = taxes.at(name);
            int tax = get_tax(cost, rate);
            if(tax > 0) {
                db.change_coins(receiver, guild.id, tax, txn);

                dpp::message msg(guild.command_channel, i18n::translate(txn, guild, "command.paid.tax",
                    dpp::user::get_mention(receiver), tax, rate*100));
                msg.set_allowed_mentions(true, false, false, false, {}, {});
                discord.message_create(msg);

                txn.commit();
            }
        }
    }
    else
    {
        // refund if command fails
        db.change_coins(user_id, guild.id, cost, txn);
        txn.commit();
    }

    co_return;
}

}
