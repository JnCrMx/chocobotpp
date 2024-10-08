module;

#include <sstream>
#include <filesystem>
#include <optional>
#include <vector>
#include <functional>

export module chocobot.utils;
import chocobot.database;
import chocobot.branding;
import spdlog;
import pqxx;
import dpp;

export namespace chocobot::utils {

std::string find_exists(const std::vector<std::optional<std::string>>& paths);
std::optional<std::string> get_env(const std::string& key);

dpp::message build_error(pqxx::transaction_base& txn, const guild& guild, const std::string& key);
dpp::message build_error(pqxx::connection& db, const guild& guild, const std::string& key);

std::optional<dpp::user> get_user(dpp::snowflake id) {
    auto u = dpp::find_user(id);
    if(u) return std::optional<dpp::user>{*u};
    return std::optional<dpp::user>{};
}

std::optional<dpp::snowflake> parse_mention(const std::string& mention);
std::string solve_mentions(const std::string& string,
    std::function<std::string(const dpp::user&)> to_string = &dpp::user::format_username,
    const std::string& fallback = "unknown user",
    std::function<std::optional<dpp::user>(dpp::snowflake)> getter = &get_user);

void replaceAll(std::string& str, const std::string& from, const std::string& to);

constexpr int default_avatar_size = 256;
std::string get_effective_avatar_url(const dpp::guild_member& member, const dpp::user& user, int size = default_avatar_size);
std::string get_effective_name(const dpp::guild_member& member, const dpp::user& user);

bool parse_message_link(const std::string& link, dpp::snowflake& guild, dpp::snowflake& channel, dpp::snowflake& message);


std::string find_exists(const std::vector<std::optional<std::string>>& paths)
{
    for(auto p : paths)
    {
        if(p && std::filesystem::exists(*p))
            return *p;
    }
    throw std::runtime_error("not found");
}

std::optional<std::string> get_env(const std::string& key)
{
    char * val = getenv( key.c_str() );
    return val == NULL ? std::optional<std::string>{} : std::string(val);
}

dpp::message build_error(pqxx::transaction_base& txn, const guild& guild, const std::string& key);
dpp::message build_error(pqxx::connection& db, const guild& guild, const std::string& key)
{
    pqxx::nontransaction txn{db};
    return build_error(txn, guild, key);
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

std::string get_effective_avatar_url(const dpp::guild_member& member, const dpp::user& user, int size)
{
    auto url = [&](){
        if(!member.avatar.to_string().empty()) {
            return dpp::utility::cdn_endpoint_url_hash({ dpp::image_type::i_jpg, dpp::image_type::i_png, dpp::image_type::i_webp, dpp::image_type::i_gif },
                "guilds/" + std::to_string(member.guild_id) + "/users/" + std::to_string(member.user_id) + "/avatars/", member.avatar.to_string(),
                dpp::image_type::i_png, size, false, member.has_animated_guild_avatar());
        }
        return user.get_avatar_url(size, dpp::image_type::i_png, false);
    }();
    spdlog::trace("Resolved avatar for user {} ({}) in guild {} to URL \"{}\"",
        user.format_username(), user.id, member.guild_id, url);
    return url;
}
std::string get_effective_name(const dpp::guild_member &member, const dpp::user &user) {
    return member.get_nickname().empty() ? user.format_username() : member.get_nickname();
}

bool parse_message_link(const std::string &link, dpp::snowflake &guild, dpp::snowflake &channel, dpp::snowflake &message)
{
    if(!link.starts_with("https://discord.com/channels/"))
        return false;
    std::string subs = link.substr(29);
    std::istringstream iss(subs);
    if(!(iss >> guild))
        return false;
    if(iss.get() != '/')
        return false;
    if(!(iss >> channel))
        return false;
    if(iss.get() != '/')
        return false;
    if(!(iss >> message))
        return false;
    return true;
}

}
