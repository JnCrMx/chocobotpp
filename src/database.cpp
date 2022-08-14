#include "database.hpp"

#include <spdlog/spdlog.h>

namespace chocobot {

void database::prepare()
{
    m_connection.prepare("create_user", "INSERT INTO coins(uid, guild, coins, last_daily, daily_streak) VALUES ($1, $2, 0, 0, 0)");
    m_connection.prepare("get_coins", "SELECT coins FROM coins WHERE uid=$1 AND guild=$2");
    m_connection.prepare("change_coins", "UPDATE coins SET coins=coins+$3 WHERE uid=$1 AND guild=$2");
    m_connection.prepare("set_coins", "UPDATE coins SET coins=$3 WHERE uid=$1 AND guild=$2");
    m_connection.prepare("get_stat", "SELECT value FROM user_stats WHERE uid=$1 AND guild=$2 AND stat=$3");
    m_connection.prepare("set_stat", "INSERT INTO user_stats(uid, guild, stat, value) VALUES($1, $2, $3, $4) ON CONFLICT (uid, guild, stat) DO UPDATE SET value=$4");
}

void database::create_user(dpp::snowflake user, dpp::snowflake guild, pqxx::work* tx)
{
    tx_helper<void>(tx, [user, guild](pqxx::work& tx){
        tx.exec_prepared0("create_user", user, guild);
    });
    spdlog::debug("Created new user entry for {} in {}.", user, guild);
}

int database::get_coins(dpp::snowflake user, dpp::snowflake guild, pqxx::work* tx)
{
    return tx_helper<int>(tx, [user, guild](pqxx::work& tx){
        return tx.exec_prepared1("get_coins", user, guild).at("coins").as<int>();
    });
}

void database::change_coins(dpp::snowflake user, dpp::snowflake guild, int amount, pqxx::work* tx)
{
    tx_helper<void>(tx, [user, guild, amount](pqxx::work& tx){
        tx.exec_prepared0("change_coins", user, guild, amount);
    });
}

void database::set_coins(dpp::snowflake user, dpp::snowflake guild, int coins, pqxx::work* tx)
{
    tx_helper<void>(tx, [user, guild, coins](pqxx::work& tx){
        tx.exec_prepared0("set_coins", user, guild, coins);
    });
}

}