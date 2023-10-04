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
    m_server.resource[path("/token/guilds")]["GET"] = [this](Res res, Req req){
        boost::asio::co_spawn(*m_server.io_service, get_guilds(res, req), boost::asio::detached);
    };
    m_server.resource[path("/([\\d]+)/guild/info")]["GET"] = [this](Res res, Req req){
        boost::asio::co_spawn(*m_server.io_service, guild_info(res, req), boost::asio::detached);
    };
    m_server.resource[path("/([\\d]+)/user/self")]["GET"] = [this](Res res, Req req){
        boost::asio::co_spawn(*m_server.io_service, get_self_user(res, req), boost::asio::detached);
    };
}

void api::prepare()
{
    m_prepared_commands.check_token = m_db.prepare("check_token", "SELECT \"user\" FROM tokens WHERE token = $1");
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

template<typename Result, typename Func, typename CompletionToken = boost::asio::use_awaitable_t<>>
auto async(Func fn, CompletionToken token = {})
{
    auto initiate = [fn]<typename Handler>(Handler&& handler){
        fn([h = std::make_shared<Handler>(std::forward<Handler>(handler))](const dpp::confirmation_callback_t& c)mutable{
            (*h)(c.get<Result>());
        });
    };
    return boost::asio::async_initiate<CompletionToken, void(Result)>(initiate, token);
}

template<typename Result, typename Func, typename... Args>
auto awaitable(Func fn, Args... args)
{
    return async<Result>(std::bind_front(fn, args...));
}

boost::asio::awaitable<void> api::get_guilds(Res res, Req req)
{
    dpp::snowflake user;
    if(auto opt = get_user(req); opt.has_value()) {
        user = opt.value();
    } else {
        res->write(StatusCode::client_error_unauthorized);
        co_return;
    }

    const auto& guild_map = co_await awaitable<dpp::guild_map>(&dpp::cluster::current_user_get_guilds, &m_chocobot->m_bot);

    nlohmann::json guilds = nlohmann::json::array();

    auto conn = m_db.acquire_connection();
    pqxx::nontransaction txn{*conn};
    for(const auto& [guild_id, guild] : guild_map)
    {
        if(!m_db.get_coins(user, guild_id, txn).has_value()) continue;
        auto& g = guilds.emplace_back();
        g["id"] = std::to_string(guild_id);
        g["name"] = guild.name;
        g["iconUrl"] = guild.get_icon_url();
    }
    res->write(nlohmann::to_string(guilds));
}

boost::asio::awaitable<void> api::guild_info(Res res, Req req)
{
    dpp::snowflake user;
    if(auto opt = get_user(req); opt.has_value()) {
        user = opt.value();
    } else {
        res->write(StatusCode::client_error_unauthorized);
        co_return;
    }

    dpp::snowflake guild_id = std::stoul(req->path_match.str(1));
    {
        auto conn = m_db.acquire_connection();
        pqxx::nontransaction txn{*conn};
        if(!m_db.get_coins(user, guild_id, txn).has_value())
        {
            res->write(StatusCode::client_error_forbidden);
            co_return;
        }
    }
    auto guild = co_await awaitable<dpp::guild>(&dpp::cluster::guild_get, &m_chocobot->m_bot, guild_id);

    std::optional<struct guild> guild_settings;
    {
        auto conn = m_db.acquire_connection();
        pqxx::nontransaction txn{*conn};
        guild_settings = m_db.get_guild(guild_id, txn);
    }
    if(!guild_settings)
    {
        res->write(StatusCode::client_error_not_found);
        co_return;
    }

    nlohmann::json guild_info{};
    guild_info["guildName"] = guild.name;
    guild_info["guildId"] = std::to_string(guild_id);
    guild_info["commandChannelId"] = std::to_string(guild_settings->command_channel);
    guild_info["commandChannelName"] = (co_await awaitable<dpp::channel>(&dpp::cluster::channel_get, &m_chocobot->m_bot, guild_settings->command_channel)).name;
    guild_info["iconUrl"] = guild.get_icon_url();

    guild_info["owner"] = {};
    dpp::guild_member owner = co_await awaitable<dpp::guild_member>(&dpp::cluster::guild_get_member, &m_chocobot->m_bot, guild_id, guild.owner_id);
    dpp::user owner_user = co_await awaitable<dpp::user_identified>(&dpp::cluster::user_get_cached, &m_chocobot->m_bot, guild.owner_id);
    guild_info["owner"]["userId"] = std::to_string(owner.user_id);
    guild_info["owner"]["tag"] = owner_user.format_username();
    guild_info["owner"]["nickname"] = owner.nickname.empty() ? owner_user.username : owner.nickname;
    guild_info["owner"]["avatarUrl"] = owner.get_avatar_url().empty() ? owner_user.get_avatar_url() : owner.get_avatar_url();

    res->write(nlohmann::to_string(guild_info));
}

boost::asio::awaitable<void> api::get_self_user(Res res, Req req)
{
    dpp::snowflake user;
    if(auto opt = get_user(req); opt.has_value()) {
        user = opt.value();
    } else {
        res->write(StatusCode::client_error_unauthorized);
        co_return;
    }

    int coins = 0;
    dpp::snowflake guild_id = std::stoul(req->path_match.str(1));
    {
        auto conn = m_db.acquire_connection();
        pqxx::nontransaction txn{*conn};
        if(auto coinsOpt = m_db.get_coins(user, guild_id, txn); coinsOpt.has_value())
        {
            coins = coinsOpt.value();
        }
        else
        {
            res->write(StatusCode::client_error_forbidden);
            co_return;
        }
    }
    std::optional<struct guild> guild_settings;
    {
        auto conn = m_db.acquire_connection();
        pqxx::nontransaction txn{*conn};
        guild_settings = m_db.get_guild(guild_id, txn);
    }
    if(!guild_settings)
    {
        res->write(StatusCode::client_error_not_found);
        co_return;
    }
    // TODO: Check operator

    dpp::guild_member member = co_await awaitable<dpp::guild_member>(&dpp::cluster::guild_get_member, &m_chocobot->m_bot, guild_id, user);
    dpp::user member_user = co_await awaitable<dpp::user_identified>(&dpp::cluster::user_get_cached, &m_chocobot->m_bot, user);

    nlohmann::json j{};
    j["userId"] = std::to_string(user);
    j["tag"] = member_user.format_username();
    j["nickname"] = member.nickname.empty() ? member_user.username : member.nickname;
    j["avatarUrl"] = member.get_avatar_url().empty() ? member_user.get_avatar_url() : member.get_avatar_url();

}

}
