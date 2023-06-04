#pragma once

#include <nlohmann/json.hpp>

namespace chocobot {

struct config
{
    std::string token;
    std::string db_uri;

    std::string bugreport_command;

    std::string api_address;
    int api_port;

    config(nlohmann::json j) : 
        token(j["token"]),
        db_uri(j["database"]),
        bugreport_command(j.value("bugreport", "false")),
        api_port(j.value("api", nlohmann::json(nlohmann::json::value_t::object)).value("port", 8080)),
        api_address(j.value("api", nlohmann::json(nlohmann::json::value_t::object)).value("address", ""))
    {}
    config() = default;
};

}