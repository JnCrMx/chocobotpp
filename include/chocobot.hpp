#pragma once

#include <chrono>
#include <dpp/dpp.h>
#include <thread>

#include "config.hpp"
#include "database.hpp"

namespace chocobot {

class chocobot
{
    public:
        chocobot(config config) : m_config(config), m_db(config.db_uri), 
            m_bot(config.token, dpp::i_default_intents | dpp::i_message_content, /*token, intents*/
                  0, 0, 1, true,  /*shards, cluster_id, maxclusters, compressed*/
                  {dpp::cp_aggressive, dpp::cp_none, dpp::cp_none}, /*policy*/
                  4, 1) /*request_threads, request_threads_raw*/
        {
            init();
        }
        ~chocobot() = default;

        void start();
        void stop();
    private:
        void init();

        void check_reminds();

        config m_config;
        dpp::cluster m_bot;
        database m_db;

        std::jthread remind_thread;
        struct {
            std::string list;
            std::string done;
        } remind_queries{};
};

}
