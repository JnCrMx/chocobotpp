#pragma once

#include <dpp/dpp.h>
#include <pqxx/pqxx>

#include "database.hpp"

namespace chocobot::utils {

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

}