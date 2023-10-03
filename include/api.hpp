#pragma once

#include "config.hpp"
#include "database.hpp"

#include <server_http.hpp>
#include <boost/asio.hpp>

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
namespace chocobot {
    class chocobot;

    class api {
        using Req = std::shared_ptr<HttpServer::Request>;
        using Res = std::shared_ptr<HttpServer::Response>;
        public:
            api(const config& config, database& db, chocobot* chocobot);

            void prepare();

            void start();
            void stop();

            constexpr static auto API_PREFIX = "/api/v1";
        private:
            std::string m_address;
            int m_port;

            HttpServer m_server{};
            std::jthread m_thread;

            chocobot* m_chocobot;
            database& m_db;
            struct {
                std::string check_token;
            } m_prepared_commands;

            std::optional<dpp::snowflake> get_user(Req req);

            void check_token(Res res, Req req);
            boost::asio::awaitable<void> get_guilds(Res res, Req req);
            boost::asio::awaitable<void> guild_info(Res res, Req req);
            boost::asio::awaitable<void> get_self_user(Res res, Req req);
    };
}
