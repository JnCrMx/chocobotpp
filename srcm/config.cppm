module;

#include <nlohmann/json.hpp>
#include <dpp/snowflake.h>

#include <unordered_set>

export module chocobot.config;
import chocobot.branding;

namespace chocobot {

export struct config
{
    std::string token;
    std::string db_uri;
    dpp::snowflake owner;

    std::string bugreport_command;

    std::string api_address;
    int api_port;
    struct cors_config {
        bool enabled = false;
        std::unordered_set<std::string> origins{};
    } api_cors;

    struct command_tax {
        dpp::snowflake receiver;
        double rate;
    };
    std::unordered_map<std::string, command_tax> taxes;

    std::string fortunes_directory;

    config(nlohmann::json j) :
        token(j["token"]),
        db_uri(j["database"]),
        owner(j.value<uint64_t>("owner", {})),
        bugreport_command(j.value("bugreport", "false")),
        api_port(j.value("api", nlohmann::json(nlohmann::json::value_t::object)).value("port", 8080)),
        api_address(j.value("api", nlohmann::json(nlohmann::json::value_t::object)).value("address", "")),
        fortunes_directory(j.value("fortunes_directory", std::string{}))
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

        if(j.contains("api") && j["api"].contains("cors")) {
            api_cors.enabled = j["api"]["cors"].value("enabled", false);
            if(j["api"]["cors"].contains("origin"))
                api_cors.origins = {j["api"]["cors"]["origin"]};
            else
                api_cors.origins = j["api"]["cors"].value("origins", std::unordered_set<std::string>{});
        }
    }
    config() = default;
};

}
