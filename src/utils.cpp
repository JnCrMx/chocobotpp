#include "utils.hpp"
#include "i18n.hpp"
#include "branding.hpp"

#include <dpp/dpp.h>
#include <spdlog/spdlog.h>
#include <sstream>

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

dpp::user provide_user(dpp::cluster& bot, dpp::snowflake id)
{
    auto up = dpp::find_user(id);
    if(up) return *up;
    spdlog::warn("User returned by dpp::find_user for id {} is null", id);
    return bot.user_get_sync(id);
}

std::optional<dpp::snowflake> parse_mention(const std::string &mention)
{
    if(!mention.starts_with("<@"))
        return std::optional<dpp::snowflake>{};
    if(!mention.ends_with(">"))
        return std::optional<dpp::snowflake>{};
    std::string subs = mention.substr(2, mention.size() - 2 - 1);

    dpp::snowflake flake;
    std::istringstream iss(subs);
    iss >> flake;
    return flake;
}

std::string solve_mentions(const std::string &string, 
    std::function<std::string(const dpp::user&)> to_string, const std::string& fallback,
    std::function<std::optional<dpp::user>(dpp::snowflake)> getter)
{
    std::string copy = string;

    std::string::size_type pos = 0;
    while((pos = copy.find("<@", pos)) != std::string::npos)
    {
        auto end = copy.find(">", pos);
        std::string mention = copy.substr(pos, end-pos+1);

        auto snowflake = parse_mention(mention);
        std::string str = snowflake.and_then(getter).transform(to_string).value_or(fallback);

        copy = copy.substr(0, pos) + str + copy.substr(end+1);
        break;
    }
    return copy;
}

void replaceAll(std::string &str, const std::string &from, const std::string &to)
{
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

}
