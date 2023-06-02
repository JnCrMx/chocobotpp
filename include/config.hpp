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
        bugreport_command(j["bugreport"]),
        api_port(j["api"]["port"]),
        api_address(j["api"]["address"])
    {}
    config() = default;
};

}