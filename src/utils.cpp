#include "utils.hpp"
#include "i18n.hpp"
#include "branding.hpp"

#include <dpp/dpp.h>

namespace chocobot::utils {

dpp::message build_error(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
{
    dpp::embed embed{};
    embed.set_title(i18n::translate(txn, guild, "error"));
    embed.set_color(branding::colors::error);
    embed.set_description(i18n::translate(txn, guild, key));
    return dpp::message{dpp::snowflake{}, embed};
}

dpp::message build_error(pqxx::connection& db, const guild& guild, const std::string& key)
{
    pqxx::nontransaction txn{db};
    return build_error(txn, guild, key);
}

}