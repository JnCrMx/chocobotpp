#pragma once

#include <dpp/dpp.h>

#include "config.hpp"
#include "database.hpp"

namespace chocobot {

class chocobot
{
    public:
        chocobot(config config) : m_config(config), m_db(config.db_uri), m_bot(config.token, dpp::i_default_intents | dpp::i_message_content) {
            init();
        }

        void start();
    private:
        void init();

        config m_config;
        dpp::cluster m_bot;
        database m_db;
};

}
