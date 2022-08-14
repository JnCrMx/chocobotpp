#pragma once

#include <pqxx/pqxx>

namespace chocobot {

    class database {
        public:
            database(const std::string& uri) : m_connection(uri) {
                m_connection.set_session_var("application_name", "ChocoBot");
            }

            void prepare();

            pqxx::connection m_connection;
    };

}