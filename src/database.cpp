#include "database.hpp"

#include <mutex>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/chrono.h>
#include <dpp/snowflake.h>

namespace chocobot {

void database::prepare()
{
    for(auto& conn : m_connections)
    {
        conn->prepare("create_user", "INSERT INTO coins(uid, guild, coins, last_daily, daily_streak) VALUES ($1, $2, 0, 0, 0)");
        conn->prepare("get_coins", "SELECT coins FROM coins WHERE uid=$1 AND guild=$2");
        conn->prepare("change_coins", "UPDATE coins SET coins=coins+$3 WHERE uid=$1 AND guild=$2");
        conn->prepare("set_coins", "UPDATE coins SET coins=$3 WHERE uid=$1 AND guild=$2");
        conn->prepare("get_stat", "SELECT value FROM user_stats WHERE uid=$1 AND guild=$2 AND stat=$3");
        conn->prepare("set_stat", "INSERT INTO user_stats(uid, guild, stat, value) VALUES($1, $2, $3, $4) ON CONFLICT (uid, guild, stat) DO UPDATE SET value=$4");
        conn->prepare("get_guild", "SELECT id, prefix, command_channel, remind_channel, warning_channel, poll_channel, language, timezone FROM guilds WHERE id=$1");
        conn->prepare("get_translation", "SELECT value FROM guild_language_overrides WHERE guild=$1 AND key=$2");
        conn->prepare("get_custom_command", "SELECT message FROM custom_commands WHERE guild=$1 AND keyword=$2");
    }
}

connection_wrapper database::acquire_connection()
{
    std::unique_lock<std::mutex> guard(m_lock);
    while(!m_signal.wait_for(guard, acquire_timeout, [this](){return !m_connections.empty();}))
    {
        spdlog::warn("Failed to acquire connection after {}", acquire_timeout);
    }

    auto conn = std::move(m_connections.front());
    m_connections.pop_front();

    spdlog::trace("Acquire connnection {} for thread {}", conn->get_var("application_name"), std::hash<std::thread::id>{}(std::this_thread::get_id()));

    return connection_wrapper(this, std::move(conn));
}

connection_wrapper::~connection_wrapper()
{
    m_db->return_connection(std::move(m_connection));
}

void database::return_connection(std::unique_ptr<pqxx::connection>&& conn)
{
    {
        std::lock_guard<std::mutex> guard(m_lock);
        spdlog::trace("Returned connnection {} for thread {}", conn->get_var("application_name"), std::hash<std::thread::id>{}(std::this_thread::get_id()));
        m_connections.push_back(std::move(conn));
    }
    m_signal.notify_one();
}

std::string database::prepare(const std::string& name, const std::string& sql)
{
    std::string uname = m_main_connection->adorn_name(name);
    for(auto& conn : m_connections)
    {
        conn->prepare(uname, sql);
    }
    spdlog::debug("Prepared statement {}: {}", uname, sql);
    return uname;
}

void database::create_user(dpp::snowflake user, dpp::snowflake guild, pqxx::transaction_base& tx)
{
    tx.exec_prepared0("create_user", user, guild);
    spdlog::debug("Created new user entry for {} in {}.", user, guild);
}

std::optional<int> database::get_coins(dpp::snowflake user, dpp::snowflake guild, pqxx::transaction_base& tx)
{
    auto res = tx.exec_prepared("get_coins", user, guild);
    if(res.empty())
    {
        return std::nullopt;
    }
    return res.front().at("coins").as<int>();
}

void database::change_coins(dpp::snowflake user, dpp::snowflake guild, int amount, pqxx::transaction_base& tx)
{
    tx.exec_prepared0("change_coins", user, guild, amount);
}

void database::set_coins(dpp::snowflake user, dpp::snowflake guild, int coins, pqxx::transaction_base& tx)
{
    tx.exec_prepared0("set_coins", user, guild, coins);
}

std::optional<int> database::get_stat(dpp::snowflake user, dpp::snowflake guild, std::string stat, pqxx::transaction_base& tx)
{
    auto res = tx.exec_prepared("get_stat", user, guild, stat);
    if(res.empty())
    {
        return std::nullopt;
    }
    return res.front().at("value").as<int>();
}

void database::set_stat(dpp::snowflake user, dpp::snowflake guild, std::string stat, int value, pqxx::transaction_base& tx)
{
    tx.exec_prepared0("set_stat", user, guild, stat, value);
}

std::optional<guild> database::get_guild(dpp::snowflake guild, pqxx::transaction_base& tx)
{
    auto [
        id, prefix, command_channel, remind_channel, warning_channel, poll_channel, language, timezone
    ] = tx.exec_prepared1("get_guild", guild).as<dpp::snowflake, std::string, dpp::snowflake, dpp::snowflake, dpp::snowflake, dpp::snowflake, std::string, std::string>();
    struct guild g{
        id, prefix, command_channel, remind_channel, warning_channel, poll_channel, language, timezone
    };
    return g;
}

std::optional<std::string> database::get_custom_command(dpp::snowflake guild, const std::string& keyword, pqxx::transaction_base& tx)
{
    auto res = tx.exec_prepared("get_custom_command", guild, keyword);
    if(res.empty()) return std::nullopt;
    return res.front().front().as<std::string>();
}

}
