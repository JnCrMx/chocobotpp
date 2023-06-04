#include "api.hpp"
#include "chocobot.hpp"

#include <spdlog/spdlog.h>
#include <iostream>
#include <pqxx/nontransaction>

namespace chocobot {

using SimpleWeb::StatusCode;

static inline std::string path(std::string path)
{
    return "^"+std::string(api::API_PREFIX)+path+"$";
}

api::api(const config& config, database& db, chocobot* chocobot) : m_address(config.api_address), m_port(config.api_port), m_db(db), m_chocobot(chocobot)
{
    m_server.config.address = m_address;
    m_server.config.port = m_port;

    m_server.default_resource["GET"] = [](Res response, Req request)
    {
        response->write(StatusCode::client_error_not_found);
    };
    m_server.resource[path("/token/check")]["POST"] = std::bind_front(&api::check_token, this);
    m_server.resource[path("/token/guilds")]["GET"] = std::bind_front(&api::get_guilds, this);
}

void api::prepare()
{
    m_prepared_commands.check_token = m_db.prepare("check_token", "SELECT user FROM tokens WHERE token = $1");
}

void api::start()
{
    std::promise<unsigned short> server_port;
    m_thread = std::jthread([this, &server_port]() {
        m_server.start([&server_port](unsigned short port) {
            server_port.set_value(port);
        });
    });
    spdlog::info("API server running on {}:{} (requested port {})", m_address, server_port.get_future().get(), m_port);
}
void api::stop()
{
    m_server.stop();
}

std::optional<dpp::snowflake> api::get_user(Req req)
{
    auto h = req->header.find("Authorization");
    if(h == req->header.end()) return {};
    std::string header = (*h).second;
    std::istringstream iss(header);

    std::string type, token;
    iss >> type >> token;

    if(type != "Bearer") return {};

    auto conn = m_db.acquire_connection();
    pqxx::nontransaction txn{*conn};

    auto result = txn.exec_prepared(m_prepared_commands.check_token, token);
    if(result.empty()) return {};

    return result.front().front().as<dpp::snowflake>();
}

void api::check_token(Res res, Req req)
{
    std::string token = req->content.string();

    auto conn = m_db.acquire_connection();
    pqxx::nontransaction txn{*conn};

    auto result = txn.exec_prepared(m_prepared_commands.check_token, token);
    res->write(result.empty() ? "false" : "true");
}
void api::get_guilds(Res res, Req req)
{
    dpp::snowflake user;
    if(auto opt = get_user(req); opt.has_value()) {
        user = opt.value();
    } else {
        res->write(StatusCode::client_error_unauthorized);
        return;
    }

    m_chocobot->m_bot.current_user_get_guilds([res](dpp::confirmation_callback_t v){
        if(v.is_error()) {
            res->write(StatusCode::server_error_internal_server_error, v.get_error().message);
            return;
        }
        auto guilds = v.get<dpp::guild_map>();
    });
}

}
