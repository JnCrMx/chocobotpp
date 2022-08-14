#include <spdlog/spdlog.h>

#include "chocobot.hpp"

namespace chocobot {

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
}

void chocobot::start()
{
    m_bot.start(dpp::st_wait);
}

}
