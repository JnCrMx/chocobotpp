module;

#include <chrono>
#include <thread>
#include <coroutine>
#include <map>

export module chocobot;

export import chocobot.database;
export import chocobot.config;
export import dpp;
export import pqxx;
export import spdlog;

import pistache;

namespace chocobot {

export class chocobot;

export class command
{
    public:
        enum class result
        {
            success,
            user_error,
            system_error,
            deferred
        };

        template<typename OStream>
        friend OStream &operator<<(OStream& os, const result& r)
        {
            switch(r)
            {
                case command::result::success:      return os << "success";
                case command::result::user_error:   return os << "user_error";
                case command::result::system_error: return os << "system_error";
                case command::result::deferred:     return os << "deferred";
            }
        }
        static spdlog::level::level_enum result_level(const result& r)
        {
            switch(r)
            {
                case result::success:
                case result::user_error:
                case result::deferred:
                    return spdlog::level::info;
                case result::system_error:
                    return spdlog::level::warn;
            }
            return spdlog::level::warn;
        }

        virtual dpp::coroutine<bool> preflight(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&) {
            co_return true;
        }
        virtual dpp::coroutine<result> execute(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&) = 0;
        virtual dpp::coroutine<> postexecute(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&, result) {
            co_return;
        }

        virtual std::string get_name() = 0;
        virtual void prepare(chocobot&, database&) {}
        virtual ~command() {}
};

export struct command_factory
{
    using map_type = std::map<std::string, std::unique_ptr<command>>;

	public:
   		static map_type * get_map()
        {
       		if(map == nullptr)
            {
                map = new map_type;
            }
        	return map;
   		}

	private:
   		static map_type * map;
};

export template<typename T>
struct command_register : command_factory
{
    command_register(T* command)
    {
        get_map()->insert(std::make_pair(command->get_name(), std::unique_ptr<class command>(command)));
        spdlog::info("Registered command {}", command->get_name());
    }

    command_register() : command_register(new T) {}

    template<typename... Args>
    command_register(Args&&... args) : command_register(new T(std::forward(args...))) {}
};

export struct private_command_factory
{
    using map_type = std::map<std::string, std::unique_ptr<command>>;

	public:
   		static map_type * get_map()
        {
       		if(map == nullptr)
            {
                map = new map_type;
            }
        	return map;
   		}

	private:
   		static map_type * map;
};

export template<typename T>
struct private_command_register : private_command_factory
{
    private_command_register(T* command)
    {
        get_map()->insert(std::make_pair(command->get_name(), std::unique_ptr<class command>(command)));
        spdlog::info("Registered private command {}", command->get_name());
    }

    private_command_register() : private_command_register(new T) {}

    template<typename... Args>
    private_command_register(Args&&... args) : private_command_register(new T(std::forward(args...))) {}
};

export class paid_command : public command {
    public:
        virtual dpp::coroutine<bool> preflight(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&) override;
        virtual dpp::coroutine<> postexecute(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&, result) override;
        virtual int get_cost() = 0;
};

};

template<> struct fmt::formatter<chocobot::command::result> : formatter<std::string_view>
{
    template<typename FormatContext>
    auto format(chocobot::command::result r, FormatContext& ctx) const
    {
        std::string_view s = "unknown";
        switch(r)
        {
            case chocobot::command::result::success:      s = "success"; break;
            case chocobot::command::result::user_error:   s = "user_error"; break;
            case chocobot::command::result::system_error: s = "system_error"; break;
            case chocobot::command::result::deferred:     s = "deferred"; break;
        }
        return formatter<string_view>::format(s, ctx);
    }
};

namespace chocobot {

export class api {
    public:
        api(const config& config, database& db, chocobot* chocobot);

        void prepare();

        void start();
        void stop();

        constexpr static auto API_PREFIX = "/api/v1";
    private:
        void setupRoutes();

        void healthz(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);
        void token_check(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);
        void token_guilds(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);
        void guild_info(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);
        void get_self_user(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);
        void save_streaks(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);

        std::string m_address;
        int m_port;
        dpp::snowflake m_owner;
        config::cors_config m_cors_config;

        Pistache::Http::Endpoint m_endpoint;
        Pistache::Rest::Router m_router;

        chocobot* m_chocobot;
        dpp::cluster& m_cluster;
        database& m_db;
        struct {
            std::string check_token;
            std::string save_streaks;
        } m_prepared_commands;
};

export class chocobot
{
    public:
        chocobot(config config) : m_config(config), m_db(config.db_uri),
            m_bot(config.token, dpp::i_default_intents | dpp::i_message_content, /*token, intents*/
                  0, 0, 1, true,  /*shards, cluster_id, maxclusters, compressed*/
                  {dpp::cp_aggressive, dpp::cp_none, dpp::cp_none}, /*policy*/
                  4, 1) /*request_threads, request_threads_raw*/,
            m_api(m_config, m_db, this)
        {
            init();
        }
        ~chocobot() = default;

        void start();
        void stop();

        const config& cfg() { return m_config; }
    private:
        void init();

        void check_reminds();

        friend api;

        config m_config;
        dpp::cluster m_bot;
        database m_db;

        std::jthread remind_thread;
        struct {
            std::string list;
            std::string done;
        } remind_queries{};

        api m_api;
};

}
