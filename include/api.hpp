#pragma once

#include "config.hpp"
#include "database.hpp"

#include <pistache/async.h>
#include <pistache/http.h>
#include <pistache/endpoint.h>
#include <pistache/router.h>

#include <dpp/cluster.h>

namespace chocobot {
    class chocobot;

    class api {
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
}
