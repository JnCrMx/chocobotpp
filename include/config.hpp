#pragma once

#include <nlohmann/json.hpp>

namespace chocobot {

struct config
{
    std::string token;
    std::string db_uri;

    config(nlohmann::json j) : 
        token(j["token"]),
        db_uri(j["database"])
    {}
};

}