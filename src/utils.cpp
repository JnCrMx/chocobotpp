module;

#include <sstream>
#include <filesystem>
#include <optional>

module chocobot.utils;

import chocobot.branding;
import chocobot.i18n;
import pqxx;
import dpp;
import spdlog;

namespace chocobot::utils {

dpp::message build_error(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
{
    dpp::embed embed{};
    embed.set_title(i18n::translate(txn, guild, "error"));
    embed.set_color(branding::colors::error);
    embed.set_description(i18n::translate(txn, guild, key));
    return dpp::message{dpp::snowflake{}, embed};
}

}
