#include "api.hpp"
#include "chocobot.hpp"

#include <spdlog/spdlog.h>
#include <pqxx/nontransaction>

namespace chocobot {

using namespace Pistache;
using namespace std::literals;

class XUserID : public Http::Header::Header {
    public:
        NAME("X-User-ID")

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

api::api(const config& config, database& db, chocobot* chocobot) :
    m_chocobot(chocobot), m_cluster(chocobot->m_bot), m_db(db),
    m_port(config.api_port), m_owner(config.owner),
    m_endpoint(Address(IP::any(), m_port))
{
    auto opts = Http::Endpoint::options()
        .threads(static_cast<int>(4))
        .flags(Tcp::Options::ReuseAddr);
    m_endpoint.init(opts);

    setupRoutes();
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

    constexpr auto do_cors = [](Http::ResponseWriter& resp){
        resp.headers().add<Http::Header::AccessControlAllowMethods>("GET, POST, PUT, DELETE, PATCH");
        resp.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        resp.headers().add<Http::Header::AccessControlAllowHeaders>("authorization");
    };

    m_router.addMiddleware([do_cors](Http::Request& req, Http::ResponseWriter& resp)->bool {
        do_cors(resp);
        return true;
    });
    m_router.addCustomHandler([do_cors](const Rest::Request& request, Http::ResponseWriter response){
        if(request.method() != Http::Method::Options || !request.resource().starts_with(API_PREFIX))
            return Route::Result::Failure;
        do_cors(response);
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
            guild_info["owner"]["nickname"] = owner.nickname.empty() ? owner_user.username : owner.nickname;
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
        j["nickname"] = member.nickname.empty() ? member_user.username : member.nickname;
        j["avatarUrl"] = member.get_avatar_url().empty() ? member_user.get_avatar_url() : member.get_avatar_url();
        j["coins"] = coins;
        j["onlineStatus"] = "ONLINE";
        j["timeJoined"] = member.joined_at;
        j["role"] = roles.contains(member.roles.front()) ? roles.at(member.roles.front()).name : "";
        j["roleColor"] = roles.contains(member.roles.front()) ? roles.at(member.roles.front()).colour : 0u;
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
