module;

#include <deque>
#include <optional>
#include <string>
#include <mutex>
#include <memory>
#include <set>
#include <condition_variable>

#include <dpp/snowflake.h>
#include <pqxx/pqxx>

export module chocobot.database;

export namespace pqxx
{
    template<> struct nullness<dpp::snowflake> : no_null<dpp::snowflake> {};
    template<> struct string_traits<dpp::snowflake>
    {
        static dpp::snowflake from_string(std::string_view text)
        {
            return dpp::snowflake{string_traits<uint64_t>::from_string(text)};
        }
        static zview to_buf(char *begin, char *end, dpp::snowflake const &value)
        {
            return string_traits<uint64_t>::to_buf(begin, end, static_cast<uint64_t>(value));
        }
        static char *into_buf(char *begin, char *end, dpp::snowflake const &value)
        {
            return string_traits<uint64_t>::into_buf(begin, end, static_cast<uint64_t>(value));
        }
        static size_t size_buffer(dpp::snowflake const &value) noexcept
        {
            return string_traits<uint64_t>::size_buffer(static_cast<uint64_t>(value));
        }
    };
}

export namespace chocobot {

    struct guild
    {
        dpp::snowflake id;
        std::string prefix;
        dpp::snowflake command_channel;
        dpp::snowflake remind_channel;
        dpp::snowflake warning_channel;
        dpp::snowflake poll_channel;
        std::string language;
        std::string timezone;
        std::set<dpp::snowflake> operators{};
        std::set<dpp::snowflake> muted_channels{};
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
            database(const std::string& uri, unsigned int count = default_connection_count) {
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
            static auto work(F consumer, pqxx::connection& connection) ->
                std::enable_if_t<std::is_same_v<decltype(consumer(std::declval<pqxx::transaction_base&>())), void>, void>
            {
                pqxx::work work(connection);
                consumer(work);
                work.commit();
            }

            void create_user(dpp::snowflake user, dpp::snowflake guild, pqxx::transaction_base& tx);

            std::optional<int> get_coins(dpp::snowflake user, dpp::snowflake guild, pqxx::transaction_base& tx);

            void change_coins(dpp::snowflake user, dpp::snowflake guild, int amount, pqxx::transaction_base& tx);
            void set_coins(dpp::snowflake user, dpp::snowflake guild, int coins, pqxx::transaction_base& tx);

            std::optional<int> get_stat(dpp::snowflake user, dpp::snowflake guild, std::string stat, pqxx::transaction_base& tx);
            void set_stat(dpp::snowflake user, dpp::snowflake guild, std::string stat, int value, pqxx::transaction_base& tx);

            std::optional<guild> get_guild(dpp::snowflake guild, pqxx::transaction_base& tx);

            std::optional<std::string> get_custom_command(dpp::snowflake guild, const std::string& keyword, pqxx::transaction_base& tx);
            std::optional<std::tuple<std::string, std::string>> get_command_alias(dpp::snowflake guild, const std::string& keyword, pqxx::transaction_base& tx);

            connection_wrapper acquire_connection();
            void return_connection(std::unique_ptr<pqxx::connection>&&);

            pqxx::connection* m_main_connection;
        protected:
            std::mutex m_lock;
            std::condition_variable m_signal;
            std::deque<std::unique_ptr<pqxx::connection>> m_connections{};

            static constexpr auto acquire_timeout = std::chrono::seconds(10);
            static constexpr auto default_connection_count = 10;
    };
}
