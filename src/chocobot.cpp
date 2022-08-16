#include <spdlog/spdlog.h>
#include <sstream>
#include <array>
#include <algorithm>

#include "chocobot.hpp"
#include "command.hpp"

namespace chocobot {

command_factory::map_type* command_factory::map = nullptr;

void chocobot::init()
{
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
        bool result = cmd->execute(*this, *connection, m_db, m_bot, guild, event, iss);
        if(result)
        {
            spdlog::info("Command {} from user {} in guild {} succeeded.", command, event.msg.author.format_username(), event.msg.guild_id);
        }
        else
        {
            spdlog::warn("Command {} from user {} in guild {} failed.", command, event.msg.author.format_username(), event.msg.guild_id);
        }
    });
    m_bot.on_message_reaction_add([this](const dpp::message_reaction_add_t& event){
        constexpr std::array forbidden_emojis = {"🍞", "🥖"};
        if(std::find(forbidden_emojis.begin(), forbidden_emojis.end(), event.reacting_emoji.name) != forbidden_emojis.end())
        {
            m_bot.message_delete_reaction(event.message_id, event.reacting_channel->id, event.reacting_user.id, event.reacting_emoji.name);
        }
    });
}

void chocobot::start()
{
    m_bot.start(dpp::st_wait);
}

}
