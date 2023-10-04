#pragma once

#include "branding.hpp"

#include <nlohmann/json.hpp>
#include <dpp/snowflake.h>

namespace chocobot {

struct config
{
    std::string token;
    std::string db_uri;

    std::string bugreport_command;

    std::string api_address;
    int api_port;

    struct command_tax {
        dpp::snowflake receiver;
        double rate;
    };
    std::unordered_map<std::string, command_tax> taxes;

    config(nlohmann::json j) : 
        token(j["token"]),
        db_uri(j["database"]),
        bugreport_command(j.value("bugreport", "false")),
        api_port(j.value("api", nlohmann::json(nlohmann::json::value_t::object)).value("port", 8080)),
    {
        double tax_rate = j.value("tax_rate", branding::default_tax_rate);
        for(const auto& [key, value] : j["taxes"].items())
        {
            if(value.is_object()) {
                taxes[key] = {value.at("receiver").get<uint64_t>(), value.value("rate", tax_rate)};
            } else {
                taxes[key] = {value.get<uint64_t>(), tax_rate};
            }
        }
    }
    config() = default;
};

}
