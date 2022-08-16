#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <condition_variable>

#include <dpp/snowflake.h>
#include <pqxx/pqxx>

namespace chocobot {

    struct guild
    {
        dpp::snowflake id;
        std::string prefix;
        dpp::snowflake command_channel;
        dpp::snowflake remind_channel;
        dpp::snowflake warning_channel;
        dpp::snowflake poll_channel;
        std::string language;
        std::vector<dpp::snowflake> operators{};
        std::vector<dpp::snowflake> muted_channels{};
    };

    class database;
    class connection_wrapper
    {
        private:
            database* m_db;
            std::unique_ptr<pqxx::connection> m_connection;
        public:
        connection_wrapper(database* db, std::unique_ptr<pqxx::connection>&& connection) : 
            m_db(db),
            m_connection(std::move(connection))
        {
        }
        ~connection_wrapper();

        pqxx::connection& operator*()
        {
            return *m_connection.get();
        }
    };

    class database {
        public:
            database(const std::string& uri, unsigned int count = 5) {
                for(int i=0; i<count; i++)
                {
                    std::unique_ptr<pqxx::connection> conn = std::make_unique<pqxx::connection>(uri);
                    conn->set_session_var("application_name", "ChocoBot #"+std::to_string(i));
                    m_connections.push_back(std::move(conn));
                }
                m_main_connection = m_connections.front().get();
            }

            void prepare();
            std::string prepare(const std::string& name, const std::string& sql);

            template<typename F>
            static auto work(F consumer, pqxx::connection& connection) -> decltype(consumer(std::declval<pqxx::transaction_base&>()))
            {
                pqxx::work work(connection);
                auto ret = consumer(work);
                work.commit();
                return ret;
            }

            template<typename F>
            static auto work(F consumer, pqxx::connection& connection) -> std::enable_if_t<std::is_same_v<decltype(consumer(std::declval<pqxx::transaction_base&>())), void>, void>
            {
                pqxx::work work(connection);
                consumer(work);
                work.commit();
            }

            void create_user(dpp::snowflake user, dpp::snowflake guild, pqxx::transaction_base& tx);

            int get_coins(dpp::snowflake user, dpp::snowflake guild, pqxx::transaction_base& tx);

            void change_coins(dpp::snowflake user, dpp::snowflake guild, int amount, pqxx::transaction_base& tx);
            void set_coins(dpp::snowflake user, dpp::snowflake guild, int coins, pqxx::transaction_base& tx);

            int get_stat(dpp::snowflake user, dpp::snowflake guild, std::string stat, pqxx::transaction_base& tx);
            void set_stat(dpp::snowflake user, dpp::snowflake guild, std::string stat, int value, pqxx::transaction_base& tx);

            std::optional<guild> get_guild(dpp::snowflake guild, pqxx::transaction_base& tx);

            connection_wrapper acquire_connection();
            void return_connection(std::unique_ptr<pqxx::connection>&&);

            pqxx::connection* m_main_connection;
        protected:
            std::mutex m_lock;
            std::condition_variable m_signal;
            std::deque<std::unique_ptr<pqxx::connection>> m_connections{};

            static constexpr auto acquire_timeout = std::chrono::seconds(10);
    };
}