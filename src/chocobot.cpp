#include <spdlog/spdlog.h>
#include <sstream>

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
    if(m_db.m_connection.is_open())
    {
        spdlog::info("Connected to database!");
    }
    else
    {
        spdlog::warn("Database connection (\"{}\") is not open", m_db.m_connection.connection_string());
    }
    m_db.prepare();

    for(auto& [name, cmd] : *command_factory::get_map())
    {
        cmd->prepare();
        spdlog::info("Prepared command {}", name);
    }

    m_bot.on_message_create([this](const dpp::message_create_t& event){
        if(event.msg.author.is_bot())
            return;
        std::istringstream iss(event.msg.content);
        std::string command;
        iss >> command;

        guild guild = m_db.get_guild(event.msg.guild_id);
        if(!command.starts_with(guild.prefix))
            return;
        command = command.substr(guild.prefix.size());
        if(!command_factory::get_map()->contains(command))
            return;
        auto& cmd = command_factory::get_map()->at(command);
        bool result = cmd->execute(*this, m_bot, guild, event, iss);
        if(result)
        {
            spdlog::info("Command {} from user {} in guild {} succeeded.", command, event.msg.author.format_username(), event.msg.guild_id);
        }
        else
        {
            spdlog::warn("Command {} from user {} in guild {} failed.", command, event.msg.author.format_username(), event.msg.guild_id);
        }
    });
}

void chocobot::start()
{
    m_bot.start(dpp::st_wait);
}

}
