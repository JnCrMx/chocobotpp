#pragma once

#include <dpp/snowflake.h>
#include <dpp/user.h>
#include <dpp/cache.h>
#include <dpp/message.h>
#include <pqxx/transaction_base>
#include <pqxx/connection>

#include "database.hpp"

namespace chocobot::utils {

std::string find_exists(const std::vector<std::optional<std::string>>& paths);
std::optional<std::string> get_env(const std::string& key);

dpp::message build_error(pqxx::transaction_base& txn, const guild& guild, const std::string& key);
dpp::message build_error(pqxx::connection& db, const guild& guild, const std::string& key);

std::optional<dpp::snowflake> parse_mention(const std::string& mention);
std::string solve_mentions(const std::string& string,
    std::function<std::string(const dpp::user&)> to_string = &dpp::user::format_username,
    const std::string& fallback = "unknown user",
    std::function<std::optional<dpp::user>(dpp::snowflake)> getter = [](dpp::snowflake s){
        auto u = dpp::find_user(s);
        if(u) return std::optional<dpp::user>{*u};
        return std::optional<dpp::user>{};
    });

void replaceAll(std::string& str, const std::string& from, const std::string& to);

static constexpr int default_avatar_size = 256;
std::string get_effective_avatar_url(const dpp::guild_member& member, const dpp::user& user, int size = default_avatar_size);
std::string get_effective_name(const dpp::guild_member& member, const dpp::user& user);

bool parse_message_link(const std::string& link, dpp::snowflake& guild, dpp::snowflake& channel, dpp::snowflake& message);

}
