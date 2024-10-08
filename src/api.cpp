module;

#include "pistache_macros.hpp"

#include <coroutine>
#include <set>
#include <string>
#include <cstdint>
#include <optional>

module chocobot;

import pqxx;
import spdlog;
import pistache;

namespace chocobot {

using namespace Pistache;
using namespace std::literals;

namespace headers {
    class XUserID : public Http::Header::Header {
        public:
            NAME("X-User-ID")

            XUserID() = default;
            XUserID(dpp::snowflake uid) : m_uid(uid) {}

            void parse(const std::string& data) override {
                m_uid = std::stoull(data);
            }
            void write(std::ostream& os) const override {
                os << m_uid;
            }

            dpp::snowflake uid() const {
                return m_uid;
            }
        private:
            dpp::snowflake m_uid;
    };

    class Origin : public Http::Header::Header {
        public:
            NAME("Origin")

            Origin() = default;
            Origin(std::string origin) : m_origin(std::move(origin)) {}

            void parse(const std::string& data) override {
                m_origin = data;
            }
            void write(std::ostream& os) const override {
                os << m_origin;
            }

            const std::string& value() const {
                return m_origin;
            }
        private:
            std::string m_origin;
    };

    class Vary : public Http::Header::Header {
        public:
            NAME("Vary")

            Vary() = default;
            Vary(std::string vary) : m_vary(std::move(vary)) {}

            void parse(const std::string& data) override {
                m_vary = data;
            }
            void write(std::ostream& os) const override {
                os << m_vary;
            }

            const std::string& value() const {
                return m_vary;
            }
        private:
            std::string m_vary;
    };

    using Pistache::Http::Header::Registrar;
    RegisterHeader(XUserID);
    RegisterHeader(Origin);
    RegisterHeader(Vary);
}
using XUserID = headers::XUserID;

api::api(const config& config, database& db, chocobot* chocobot) :
    m_chocobot(chocobot), m_cluster(chocobot->m_bot), m_db(db),
    m_port(config.api_port), m_owner(config.owner), m_cors_config(config.api_cors),
    m_endpoint(Address(IP::any(), m_port))
{
    auto opts = Http::Endpoint::options()
        .threads(static_cast<int>(4))
        .flags(Tcp::Options::ReuseAddr);
    m_endpoint.init(opts);

    setupRoutes();

    if(m_cors_config.enabled) {
        if(m_cors_config.origins.empty()) {
            spdlog::warn("CORS is enabled but no origins are specified.");
        }
        else if(m_cors_config.origins.contains("*")) {
            spdlog::warn("CORS is enabled and all origins are allowed.");
        }
        else {
            spdlog::info("CORS is enabled and {} origin{} allowed.", m_cors_config.origins.size(),
                m_cors_config.origins.size() == 1 ? " is" : "s are");
            if(spdlog::should_log(spdlog::level::debug)) {
                spdlog::debug("The following origins are allowed:");
                for(const auto& origin : m_cors_config.origins) {
                    spdlog::debug("  {}", origin);
                }
            }
        }
    }
}

void api::prepare() {
    m_prepared_commands.check_token = m_db.prepare("check_token", "SELECT \"user\" FROM tokens WHERE token = $1");
    m_prepared_commands.save_streaks = m_db.prepare("save_streaks", "UPDATE coins SET last_daily=GREATEST(EXTRACT(EPOCH FROM (CURRENT_DATE)::TIMESTAMP - INTERVAL '1 day' + INTERVAL '12 hours')::BIGINT*1000, last_daily);");
}

static const std::set<std::string> public_routes = {
   api:: API_PREFIX+"/token/check"s, api::API_PREFIX+"/healthz"s
};
static const std::string admin_prefix = "/admin";

void api::setupRoutes() {
    using namespace Pistache::Rest;

    auto do_cors = [this](const Http::Header::Collection& headers, Http::ResponseWriter& resp){
        if(!m_cors_config.enabled) return;

        // TODO: Add support for Cross-Origin-Resource-Policy header
        resp.headers().add<Http::Header::AccessControlAllowMethods>("GET, POST, PUT, DELETE, PATCH");
        resp.headers().add<Http::Header::AccessControlAllowHeaders>("authorization");

        if(m_cors_config.origins.contains("*")) {
            resp.headers().add<Http::Header::AccessControlAllowOrigin>("*");
            return;
        }
        if(m_cors_config.origins.size() == 1) {
            resp.headers().add<Http::Header::AccessControlAllowOrigin>(*m_cors_config.origins.begin());
            return;
        } else {
            resp.headers().add<headers::Vary>("Origin");

            if(!headers.has<headers::Origin>())
                return;
            auto origin = headers.get<headers::Origin>();
            if(m_cors_config.origins.contains(origin->value())) {
                resp.headers().add<Http::Header::AccessControlAllowOrigin>(origin->value());
                return;
            }
        }
    };
    m_router.addMiddleware([do_cors](Http::Request& req, Http::ResponseWriter& resp)->bool {
        do_cors(req.headers(), resp);
        return true;
    });
    m_router.addCustomHandler([do_cors](const Rest::Request& request, Http::ResponseWriter response){
        if(request.method() != Http::Method::Options || !request.resource().starts_with(API_PREFIX))
            return Route::Result::Failure;
        do_cors(request.headers(), response);
        response.send(Http::Code::Ok);
        return Route::Result::Ok;
    });
    m_router.addMiddleware([this](Http::Request& req, Http::ResponseWriter& resp)->bool {
        if(!req.resource().starts_with(API_PREFIX) ||
            req.method() == Http::Method::Options ||
            public_routes.contains(req.resource())) {
            return true;
        }

        if(!req.headers().has<Http::Header::Authorization>()) {
            resp.send(Http::Code::Unauthorized);
            return false;
        }
        auto auth = req.headers().get<Http::Header::Authorization>();
        if(!auth->hasMethod<Http::Header::Authorization::Method::Bearer>()) {
            resp.send(Http::Code::Unauthorized);
            return false;
        }
        auto token = auth->value().substr(std::string::traits_type::length("Bearer "));

        auto conn = m_db.acquire_connection();
        pqxx::nontransaction txn{*conn};

        auto result = txn.exec_prepared(m_prepared_commands.check_token, token);
        if(result.empty()) {
            resp.send(Http::Code::Unauthorized);
            return false;
        }
        auto user = result.front().front().as<dpp::snowflake>();

        req.headers().add<XUserID>(user);
        return true;
    });
    m_router.addMiddleware([this](Http::Request& req, Http::ResponseWriter& resp)->bool {
        if(!req.resource().starts_with(API_PREFIX+admin_prefix) ||
            req.method() == Http::Method::Options ||
            public_routes.contains(req.resource())) {
            return true;
        }
        if(m_owner.empty()) {
            resp.send(Http::Code::Forbidden);
            return false;
        }

        dpp::snowflake user = req.headers().get<XUserID>()->uid();
        if(user != m_owner) {
            resp.send(Http::Code::Forbidden);
            return false;
        }
        return true;
    });

    Routes::Get(m_router, API_PREFIX+"/healthz"s, Routes::bind(&api::healthz, this));
    Routes::Post(m_router, API_PREFIX+"/token/check"s, Routes::bind(&api::token_check, this));
    Routes::Get(m_router, API_PREFIX+"/token/guilds"s, Routes::bind(&api::token_guilds, this));
    Routes::Get(m_router, API_PREFIX+"/:id/guild/info"s, Routes::bind(&api::guild_info, this));
    Routes::Get(m_router, API_PREFIX+"/:id/user/self"s, Routes::bind(&api::get_self_user, this));
    Routes::Post(m_router, API_PREFIX+admin_prefix+"/save_streaks"s, Routes::bind(&api::save_streaks, this));
}

void api::start() {
    m_endpoint.setHandler(m_router.handler());
    m_endpoint.serveThreaded();

    spdlog::info("API is listening on port {}", m_port);
}

void api::stop() {
    m_endpoint.shutdown();
}

void api::healthz(const Rest::Request& request, Http::ResponseWriter response) {
    nlohmann::json res{};

    bool error = false;

    res["discord"]["ping"] = static_cast<unsigned long>(m_cluster.rest_ping*1000.0);
    res["discord"]["username"] = m_cluster.me.format_username();
    res["discord"]["uptime"] = m_cluster.uptime().to_msecs();
    error |= !(res["database"]["connected"] = m_db.m_main_connection->is_open());

    response.send(Http::Code::Ok, nlohmann::to_string(res));
}

void api::token_check(const Rest::Request& request, Http::ResponseWriter response) {
    std::string token = request.body();

    auto conn = m_db.acquire_connection();
    pqxx::nontransaction txn{*conn};

    auto result = txn.exec_prepared(m_prepared_commands.check_token, token);
    response.send(Http::Code::Ok, result.empty() ? "false" : "true");
}

void api::token_guilds(const Rest::Request& request, Http::ResponseWriter response) {
    dpp::snowflake user = request.headers().get<XUserID>()->uid();

    [this, user](Http::ResponseWriter response) mutable -> dpp::job {
        const auto& guild_map_res = co_await m_cluster.co_current_user_get_guilds();
        if(guild_map_res.is_error()) {
            response.send(Http::Code::Internal_Server_Error, guild_map_res.get_error().message);
            co_return;
        }
        auto guild_map = guild_map_res.get<dpp::guild_map>();

        nlohmann::json guilds = nlohmann::json::array();

        auto conn = m_db.acquire_connection();
        pqxx::nontransaction txn{*conn};
        for(const auto& [guild_id, guild] : guild_map)
        {
            if(!m_db.get_coins(user, guild_id, txn).has_value() && guild.owner_id != user) continue;
            auto& g = guilds.emplace_back();
            g["id"] = std::to_string(guild_id);
            g["name"] = guild.name;
            g["iconUrl"] = guild.get_icon_url();
        }

        response.send(Http::Code::Ok, nlohmann::to_string(guilds));
        co_return;
    }(std::move(response));
}

void api::guild_info(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
    dpp::snowflake user = request.headers().get<XUserID>()->uid();
    dpp::snowflake guild_id = request.param(":id").as<unsigned long>();

    [this, user, guild_id](Http::ResponseWriter response) mutable -> dpp::job {
        {
            auto conn = m_db.acquire_connection();
            pqxx::nontransaction txn{*conn};
            if(!m_db.get_coins(user, guild_id, txn).has_value()) {
                response.send(Http::Code::Forbidden, "Not in guild");
                co_return;
            }
            auto guild_res = co_await m_cluster.co_guild_get(guild_id);
            if(guild_res.is_error()) {
                response.send(Http::Code::Not_Found, "Guild not found");
                co_return;
            }
            auto guild = guild_res.get<dpp::guild>();

            std::optional<struct guild> guild_settings;
            {
                auto conn = m_db.acquire_connection();
                pqxx::nontransaction txn{*conn};
                guild_settings = m_db.get_guild(guild_id, txn);
            }
            if(!guild_settings) {
                response.send(Http::Code::Not_Found, "Guild not found");
                co_return;
            }
            nlohmann::json guild_info{};
            guild_info["guildName"] = guild.name;
            guild_info["guildId"] = std::to_string(guild_id);
            guild_info["commandChannelId"] = std::to_string(guild_settings->command_channel);
            guild_info["commandChannelName"] = (co_await m_cluster.co_channel_get(guild_settings->command_channel)).get<dpp::channel>().name;
            guild_info["iconUrl"] = guild.get_icon_url();

            guild_info["owner"] = {};
            dpp::guild_member owner = (co_await m_cluster.co_guild_get_member(guild_id, guild.owner_id)).get<dpp::guild_member>();
            dpp::user owner_user = (co_await m_cluster.co_user_get_cached(guild.owner_id)).get<dpp::user_identified>();
            guild_info["owner"]["userId"] = std::to_string(owner.user_id);
            guild_info["owner"]["tag"] = owner_user.format_username();
            guild_info["owner"]["nickname"] = owner.get_nickname().empty() ? owner_user.username : owner.get_nickname();
            guild_info["owner"]["avatarUrl"] = owner.get_avatar_url().empty() ? owner_user.get_avatar_url() : owner.get_avatar_url();

            response.send(Http::Code::Ok, nlohmann::to_string(guild_info));
        }
    }(std::move(response));
}

void api::get_self_user(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
    dpp::snowflake user = request.headers().get<XUserID>()->uid();
    dpp::snowflake guild_id = request.param(":id").as<unsigned long>();

    [this, user, guild_id](Http::ResponseWriter response) mutable -> dpp::job {
        int coins = 0;
        {
            auto conn = m_db.acquire_connection();
            pqxx::nontransaction txn{*conn};
            if(auto coinsOpt = m_db.get_coins(user, guild_id, txn); coinsOpt.has_value()) {
                coins = coinsOpt.value();
            } else {
                response.send(Http::Code::Forbidden, "Not in guild");
                co_return;
            }
        }
        std::optional<struct guild> guild_settings;
        {
            auto conn = m_db.acquire_connection();
            pqxx::nontransaction txn{*conn};
            guild_settings = m_db.get_guild(guild_id, txn);
        }
        if(!guild_settings) {
            response.send(Http::Code::Not_Found, "Guild not found");
            co_return;
        }
        // TODO: Check operator

        dpp::guild_member member = (co_await m_cluster.co_guild_get_member(guild_id, user)).get<dpp::guild_member>();
        dpp::user member_user = (co_await m_cluster.co_user_get_cached(user)).get<dpp::user_identified>();
        dpp::role_map roles = (co_await m_cluster.co_roles_get(guild_id)).get<dpp::role_map>();
        dpp::guild guild = (co_await m_cluster.co_guild_get(guild_id)).get<dpp::guild>();

        nlohmann::json j{};
        j["userId"] = std::to_string(user);
        j["tag"] = member_user.format_username();
        j["nickname"] = member.get_nickname().empty() ? member_user.username : member.get_nickname();
        j["avatarUrl"] = member.get_avatar_url().empty() ? member_user.get_avatar_url() : member.get_avatar_url();
        j["coins"] = coins;
        j["onlineStatus"] = "ONLINE";
        j["timeJoined"] = member.joined_at;
        j["role"] = roles.contains(member.get_roles().front()) ? roles.at(member.get_roles().front()).name : "";
        j["roleColor"] = roles.contains(member.get_roles().front()) ? roles.at(member.get_roles().front()).colour : 0u;
        j["operator"] = guild.owner_id == user;

        response.send(Http::Code::Ok, nlohmann::to_string(j));
        co_return;
    }(std::move(response));
}

void api::save_streaks(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
    [this](Http::ResponseWriter response) mutable -> dpp::job {
        auto conn = m_db.acquire_connection();
        pqxx::work txn{*conn};
        txn.exec_prepared(m_prepared_commands.save_streaks);
        txn.commit();
        response.send(Http::Code::Ok);
        co_return;
    }(std::move(response));
}

}
