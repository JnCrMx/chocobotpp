#include <spdlog/spdlog.h>
#include <sstream>
#include <array>
#include <algorithm>
#include <date/date.h>
#include <date/tz.h>

#include "chocobot.hpp"
#include "command.hpp"
#include "i18n.hpp"
#include "message.h"

namespace chocobot {

command_factory::map_type* command_factory::map = nullptr;

void chocobot::init()
{
    i18n::init_i18n();

    m_bot.on_log([](const dpp::log_t log){
        switch(log.severity)
        {
            case dpp::loglevel::ll_trace:    spdlog::trace   (log.message); break;
            case dpp::loglevel::ll_debug:    spdlog::debug   (log.message); break;
            case dpp::loglevel::ll_info:     spdlog::info    (log.message); break;
            case dpp::loglevel::ll_warning:  spdlog::warn    (log.message); break;
            case dpp::loglevel::ll_error:    spdlog::error   (log.message); break;
            case dpp::loglevel::ll_critical: spdlog::critical(log.message); break;
        }
    });
    if(m_db.m_main_connection->is_open())
    {
        spdlog::info("Connected to database!");
    }
    else
    {
        spdlog::warn("Database connection (\"{}\") is not open", m_db.m_main_connection->connection_string());
    }
    m_db.prepare();

    for(auto& [name, cmd] : *command_factory::get_map())
    {
        cmd->prepare(*this, m_db);
        spdlog::info("Prepared command {}", name);
    }

    m_bot.on_message_create([this](const dpp::message_create_t& event){
        if(event.msg.author.is_bot())
            return;
        std::istringstream iss(event.msg.content);
        std::string command;
        iss >> command;

        auto connection = m_db.acquire_connection();
        std::optional<guild> g = database::work(std::bind_front(&database::get_guild, &m_db, event.msg.guild_id), *connection);
        if(!g)
            return;
        guild guild = g.value();

        if(!command.starts_with(guild.prefix))
            return;

        command = command.substr(guild.prefix.size());
        if(!command_factory::get_map()->contains(command))
            return;

        auto& cmd = command_factory::get_map()->at(command);
        command::result result = cmd->execute(*this, *connection, m_db, m_bot, guild, event, iss);
        spdlog::log(command::result_level(result), "Command {} (\"{}\") from user {} in guild {} returned {}.",
            command, event.msg.content, event.msg.author.format_username(), event.msg.guild_id, result);
    });
    m_bot.on_message_reaction_add([this](const dpp::message_reaction_add_t& event){
        constexpr std::array forbidden_emojis = {"🍞", "🥖"};
        if(std::find(forbidden_emojis.begin(), forbidden_emojis.end(), event.reacting_emoji.name) != forbidden_emojis.end())
        {
            m_bot.message_delete_reaction(event.message_id, event.reacting_channel->id, event.reacting_user.id, event.reacting_emoji.name);
        }
    });

    remind_queries.list = m_db.prepare("list_reminds", "SELECT * FROM reminders WHERE time <= $1 AND done = false");
    remind_queries.done = m_db.prepare("remind_done", "UPDATE reminders SET done = true WHERE id = $1");
    remind_thread = std::jthread([this](std::stop_token stop_token){
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10s);
        while(!stop_token.stop_requested())
        {
            check_reminds();
            std::this_thread::sleep_for(1min);
        }
    });
}

void chocobot::check_reminds()
{
    using system_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
    using namespace std::literals::chrono_literals;
    
    auto connection = m_db.acquire_connection();
    pqxx::work txn(*connection);

    auto now = std::chrono::system_clock::now();
    spdlog::debug("Checking reminds for time {}", now);
    auto result = txn.exec_prepared(remind_queries.list, std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    for(const auto& row : result)
    {
        int id = row.at("id").as<int>();
        dpp::snowflake uid = row.at("uid").as<dpp::snowflake>();
        dpp::snowflake guild_id = row.at("guild").as<dpp::snowflake>();
        std::string message = row.at("message").as<std::string>();
        dpp::snowflake issuer_id = row.at("issuer").as<dpp::snowflake>();
        system_time time = system_time(std::chrono::milliseconds(row.at("time").as<unsigned long>()));
        dpp::snowflake channel_id = row.at("channel").as<dpp::snowflake>();

        auto gs = m_db.get_guild(guild_id, txn);
        if(!gs.has_value()) continue;
        guild guild = gs.value();

        if(!channel_id) channel_id = guild.remind_channel;
        if(!channel_id) continue;

        dpp::user user{};
        dpp::user issuer{};

        auto userptr = dpp::find_user(uid);
        if(userptr)
        {
            user = *userptr;
        }
        else
        {
            spdlog::warn("User returned by dpp::find_user for uid {} is null", uid);
            user = m_bot.user_get_sync(uid);
        }

        auto issuerptr = dpp::find_user(issuer_id);
        if(issuerptr)
        {
            issuer = *issuerptr;
        }
        else
        {
            spdlog::warn("User returned by dpp::find_user for issuer_id {} is null", issuer_id);
            issuer = m_bot.user_get_sync(issuer_id);
        }

        std::string bot_message;
        if(uid != issuer_id)
        {
            if(message.empty())
                bot_message = i18n::translate(txn, guild, "reminder.other.plain", user.get_mention(), issuer.format_username());
            else
                bot_message = i18n::translate(txn, guild, "reminder.other.message", user.get_mention(), issuer.format_username(), message);
        }
        else
        {
            if(message.empty())
                bot_message = i18n::translate(txn, guild, "reminder.self.plain", user.get_mention());
            else
                bot_message = i18n::translate(txn, guild, "reminder.self.message", user.get_mention(), message);
        }
        if(now - time > 5min)
        {
            bot_message += " ";
            auto local = date::make_zoned(guild.timezone, time).get_local_time();
            auto forced_sys = system_time(local.time_since_epoch());
            bot_message += i18n::translate(txn, guild, "reminder.delay", forced_sys, system_time(std::chrono::duration_cast<std::chrono::milliseconds>(now-time)));
        }
        dpp::message msg{channel_id, bot_message};
        /*if(bot_message.find(user->get_mention()) != std::string::npos)
            msg.set_allowed_mentions(true, false, false, false, {uid}, {});*/
        m_bot.message_create_sync(msg);

        spdlog::debug("Completed remind {} for {} from {} at {}", id, user.format_username(), issuer.format_username(), time);

        txn.exec_prepared0(remind_queries.done, id);
    }
    txn.commit();
}

void chocobot::start()
{
    m_bot.start(dpp::st_wait);
}

}
