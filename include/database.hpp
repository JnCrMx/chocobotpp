#pragma once

#include <dpp/snowflake.h>
#include <pqxx/pqxx>
#include <functional>

namespace chocobot {

    class database {
        public:
            database(const std::string& uri) : m_connection(uri) {
                m_connection.set_session_var("application_name", "ChocoBot");
            }

            void prepare();

            void create_user(dpp::snowflake user, dpp::snowflake guild, pqxx::work* tx = nullptr);
            int get_coins(dpp::snowflake user, dpp::snowflake guild, pqxx::work* tx = nullptr);
            void change_coins(dpp::snowflake user, dpp::snowflake guild, int amount, pqxx::work* tx = nullptr);
            void set_coins(dpp::snowflake user, dpp::snowflake guild, int coins, pqxx::work* tx = nullptr);

            pqxx::connection m_connection;
        protected:
            template<typename T>
            T tx_helper(pqxx::work* tx, std::function<T(pqxx::work& tx)> consumer)
            {
                bool tx_given = tx;
                if(!tx_given) tx = new pqxx::work{m_connection};
                T t = consumer(*tx);
                if(!tx_given)
                {
                    tx->commit();
                    delete tx;
                }
                return t;
            }

            template<>
            void tx_helper(pqxx::work* tx, std::function<void(pqxx::work& tx)> consumer)
            {
                bool tx_given = tx;
                if(!tx_given) tx = new pqxx::work{m_connection};
                consumer(*tx);
                if(!tx_given)
                {
                    tx->commit();
                    delete tx;
                }
            }
    };

}