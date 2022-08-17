#include "utils.hpp"
#include "i18n.hpp"
#include "branding.hpp"

#include <dpp/dpp.h>

namespace chocobot::utils {

dpp::message build_error(pqxx::connection& db, const guild& guild, const std::string& key)
{
    dpp::embed embed{};
    embed.set_title(i18n::translate(db, guild, "error"));
    embed.set_color(branding::colors::error);
    embed.set_description(i18n::translate(db, guild, key));
    return dpp::message{dpp::snowflake{}, embed};
}

}