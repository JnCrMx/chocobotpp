module;

#include <coroutine>
#include <sstream>
#include <functional>
#include <condition_variable>
#include <thread>
#include <iomanip>
#include <date/date.h>
#include <date/tz.h>

module chocobot;

import pqxx;
import spdlog;
import chocobot.i18n;
import chocobot.utils;

namespace chocobot {

command_factory::map_type* command_factory::map = nullptr;
private_command_factory::map_type* private_command_factory::map = nullptr;

static void replace_member(std::string& str, const std::string& param, const dpp::user& user, const dpp::guild_member& member)
{
    const auto& effective_name = member.get_nickname().empty() ? user.global_name : member.get_nickname();
    utils::replaceAll(str, "$"+param+".id", user.id.str());
    utils::replaceAll(str, "$"+param+".name", user.format_username());
    utils::replaceAll(str, "$"+param+".displayname", effective_name);
    utils::replaceAll(str, "$"+param+".ping", user.get_mention());
    utils::replaceAll(str, "$"+param+".avatar", user.get_avatar_url());
    utils::replaceAll(str, "$"+param, effective_name);
}

static std::string format_custom_command(const std::string& fmt, const dpp::message_create_t& event, std::istream& args)
{
    std::string text = fmt;
    replace_member(text, "self", event.msg.author, event.msg.member);
    replace_member(text, "sender", event.msg.author, event.msg.member);
    replace_member(text, "0", event.msg.author, event.msg.member);

    const auto& mentions = event.msg.mentions;
    for(int i=0; i<mentions.size(); i++) {
        const auto& [user, member] = mentions.at(i);
        replace_member(text, std::to_string(i+1), user, member);
    }

    std::string arg;
    for(int i=0; args >> std::quoted(arg); i++) {
        utils::replaceAll(text, "$"+std::to_string(i+1), arg);
    }
    return text;
}

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
    m_api.prepare();

    for(auto& [name, cmd] : *command_factory::get_map())
    {
        cmd->prepare(*this, m_db);
        spdlog::info("Prepared command {}", name);
    }
    for(auto& [name, cmd] : *private_command_factory::get_map())
    {
        cmd->prepare(*this, m_db);
        spdlog::info("Prepared private command {}", name);
    }

    m_bot.on_message_create([this](dpp::message_create_t event) -> dpp::task<void> {
        if(event.msg.author.is_bot())
            co_return;
        std::istringstream iss(event.msg.content);
        std::string command;
        iss >> command;

        auto connection = m_db.acquire_connection();

        struct guild guild;
        bool is_guild_command = event.msg.guild_id != 0ul;
        if(is_guild_command) {
            std::optional<struct guild> g = database::work(std::bind_front(&database::get_guild, &m_db, event.msg.guild_id), *connection);
            if(!g)
                co_return;
            guild = g.value();
        }
        else {
            guild.prefix = "?";
            guild.language = "en";
            guild.timezone = "UTC";
        }

        if(!command.starts_with(guild.prefix))
            co_return;

        command = command.substr(guild.prefix.size());

        class command* cmd;
        if(is_guild_command) {
            if(auto opt = database::work(std::bind_front(&database::get_command_alias, &m_db, event.msg.guild_id, command), *connection)) {
                auto [new_command, args] = *opt;
                spdlog::trace("Transformed command alias \"{}\" to \"{}\" for user {} in guild {}.",
                    command, new_command, event.msg.author.format_username(), event.msg.guild_id);
                command = new_command;
            }

            if(auto opt = database::work(std::bind_front(&database::get_custom_command, &m_db, event.msg.guild_id, command), *connection)) {
                spdlog::log(command::result_level(command::result::success), "Running custom command {} (\"{}\") from user {} in guild {}.",
                    command, event.msg.content, event.msg.author.format_username(), event.msg.guild_id);
                event.reply(format_custom_command(*opt, event, iss));
                co_return;
            }

            if(!command_factory::get_map()->contains(command))
                co_return;
            cmd = command_factory::get_map()->at(command).get();
        } else {
            if(!private_command_factory::get_map()->contains(command))
                co_return;
            cmd = private_command_factory::get_map()->at(command).get();
        }

        try{
            bool preflight = co_await cmd->preflight(*this, *connection, m_db, m_bot, guild, event, iss);
            if(!preflight) {
                spdlog::log(spdlog::level::info, "Preflight of command {} (\"{}\") from user {} in guild {} failed.",
                    command, event.msg.content, event.msg.author.format_username(), event.msg.guild_id);
                co_return;
            }
        } catch(const std::exception& ex) {
            spdlog::log(spdlog::level::err, "Preflight of command {} (\"{}\") from user {} in guild {} threw: {}",
                command, event.msg.content, event.msg.author.format_username(), event.msg.guild_id, ex.what());
        }

        command::result result = command::result::system_error;
        try {
            result = co_await cmd->execute(*this, *connection, m_db, m_bot, guild, event, iss);
        } catch(const std::exception& ex) {
            spdlog::log(spdlog::level::err, "Command {} (\"{}\") from user {} in guild {} threw: {}",
                command, event.msg.content, event.msg.author.format_username(), event.msg.guild_id, ex.what());
        }

        try {
            co_await cmd->postexecute(*this, *connection, m_db, m_bot, guild, event, iss, result);
        } catch(const std::exception& ex) {
            spdlog::log(spdlog::level::err, "Postexecute of command {} (\"{}\") from user {} in guild {} threw: {}",
                command, event.msg.content, event.msg.author.format_username(), event.msg.guild_id, ex.what());
        }

        spdlog::log(command::result_level(result), "Command {} (\"{}\") from user {} in guild {} returned {}.",
            command, event.msg.content, event.msg.author.format_username(), event.msg.guild_id, result);
    });

    remind_queries.list = m_db.prepare("list_reminds", "SELECT * FROM reminders WHERE time <= $1 AND done = false");
    remind_queries.done = m_db.prepare("remind_done", "UPDATE reminders SET done = true WHERE id = $1");
    remind_thread = std::jthread([this](std::stop_token stop_token){
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(60s);

        std::mutex mutex;
		std::unique_lock<std::mutex> lock(mutex);
        do
        {
            check_reminds();
        }
		while(!std::condition_variable_any().wait_for(lock, stop_token, 1min, [&stop_token]{return stop_token.stop_requested();}));
    });
}

void chocobot::check_reminds()
{
    using system_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
    using namespace std::literals::chrono_literals;

    auto connection = m_db.acquire_connection();

    auto now = std::chrono::system_clock::now();
    spdlog::debug("Checking reminds for time {}", now);

    pqxx::result result;
    {
        pqxx::nontransaction txn(*connection);
        result = txn.exec_prepared(remind_queries.list, std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    }

    for(const auto& row : result)
    {
        pqxx::work txn(*connection);
        int id = row.at("id").as<int>();
        dpp::snowflake uid = row.at("uid").as<dpp::snowflake>();
        dpp::snowflake guild_id = row.at("guild").as<dpp::snowflake>();
        std::optional<std::string> message = row.at("message").as<std::optional<std::string>>();
        dpp::snowflake issuer_id = row.at("issuer").as<dpp::snowflake>();
        system_time time = system_time(std::chrono::milliseconds(row.at("time").as<unsigned long>()));
        dpp::snowflake channel_id = row.at("channel").as<dpp::snowflake>();

        auto gs = m_db.get_guild(guild_id, txn);
        if(!gs.has_value()) continue;
        guild guild = gs.value();

        if(!channel_id) channel_id = guild.remind_channel;
        if(!channel_id) continue;

        dpp::user user = m_bot.user_get_cached_sync(uid);
        dpp::user issuer = m_bot.user_get_cached_sync(issuer_id);

        std::string bot_message;
        if(uid != issuer_id)
        {
            if(!message || message->empty())
                bot_message = i18n::translate(txn, guild, "reminder.other.plain", user.get_mention(), issuer.format_username());
            else
                bot_message = i18n::translate(txn, guild, "reminder.other.message", user.get_mention(), issuer.format_username(), *message);
        }
        else
        {
            if(!message || message->empty())
                bot_message = i18n::translate(txn, guild, "reminder.self.plain", user.get_mention());
            else
                bot_message = i18n::translate(txn, guild, "reminder.self.message", user.get_mention(), *message);
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

        try {
            m_bot.message_create_sync(msg);

            spdlog::debug("Completed remind {} for {} from {} at {}", id, user.format_username(), issuer.format_username(), time);

            txn.exec_prepared0(remind_queries.done, id);
            txn.commit();
        } catch(const std::exception& ex) {
            msg.channel_id = guild.remind_channel;
            spdlog::warn("Failed to send remind in channel {}, we will try to send it in the remind channel {}", channel_id, msg.channel_id);
            try {
                m_bot.message_create_sync(msg);

                spdlog::debug("Completed remind {} for {} from {} at {}", id, user.format_username(), issuer.format_username(), time);

                txn.exec_prepared0(remind_queries.done, id);
                txn.commit();
            }
            catch(const std::exception& ex) {
                spdlog::error("Failed to send remind {} for {} from {} at {}: {}", id, user.format_username(), issuer.format_username(), time, ex.what());
            }
        }
    }
}

void chocobot::start()
{
    m_api.start();
    m_bot.start(dpp::st_wait);
}

void chocobot::stop()
{
    m_bot.shutdown();
    m_api.stop();
}

}
