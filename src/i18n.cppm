module;

#include <string>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <map>

export module chocobot.i18n;
import chocobot.database;
import chocobot.utils;
import spdlog;
import pqxx;

namespace chocobot::i18n {

constexpr std::array RESOURCE_CANDIDATES = {"./resources", "/usr/local/share/chocobot/resources", "/usr/share/chocobot/resources"};
constexpr auto ENV_RESOURCES = "CHOCOBOT_RESOURCES_PATH";
constexpr auto ENV_ROOT = "CHOCOBOT_ROOT";

export inline std::string resource_root;
std::map<std::string, std::map<std::string, std::string>> translations;

static std::string find_resource_root()
{
    std::vector paths = {utils::get_env(ENV_RESOURCES), utils::get_env(ENV_ROOT).transform([](std::string s){return s+"/resources";})};
    paths.insert(paths.end(), RESOURCE_CANDIDATES.begin(), RESOURCE_CANDIDATES.end());
    try
    {
        return utils::find_exists(paths);
    }
    catch(const std::runtime_error&)
    {
        spdlog::error("Failed to locate resource directory");
        for(const auto& p : paths)
        {
            if(p)
            {
                spdlog::debug("Tried path: {}", *p);
            }
        }
        throw;
    }
}

export void init_i18n()
{
    std::string root = find_resource_root();
    resource_root = root;

    std::ifstream langs(root+"/languages.txt");
    std::string lang;
    while(std::getline(langs, lang))
    {
        auto& map = translations[lang];
        std::ifstream trans(root+"/lang/"+lang+".lang");

        std::string line;
        while(std::getline(trans, line, '\n'))
        {
            if(line.empty()) continue;

            std::istringstream iss(line);

            std::string key, value;
            std::getline(iss, key, '=');
            std::getline(iss, value);
            utils::replaceAll(value, "\\n", "\n");

            map[key] = value;
            spdlog::trace("Loaded translation: {} -> \"{}\"", key, value);
        }
        spdlog::info("Loaded language {} with {} translations", lang, map.size());
    }
}

export std::string translate_get(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
{
    // TODO: check overrides

    std::string lang = guild.language;
    if(translations.contains(lang))
    {
        const auto& t = translations.at(lang);
        if(t.contains(key))
        {
            return t.at(key);
        }
    }

    return key; // translation not found, just return the key
}

export std::string translate_base(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
{
    std::string translation = translate_get(txn, guild, key);

    std::string::size_type pos = 0;
    while((pos = translation.find("@{", pos)) != std::string::npos)
    {
        auto end = translation.find("}", pos);
        std::string subkey = translation.substr(pos+2, end-pos-2);
        std::string trans = translate_base(txn, guild, subkey);

        translation = translation.substr(0, pos) + trans + translation.substr(end+1);
    }

    return translation;
}

export template <typename... T>
std::string translate(pqxx::transaction_base& txn, const guild& guild, const std::string& key, T&&... args)
{
    std::string translation = translate_base(txn, guild, key);
    return fmt::vformat(translation, fmt::make_format_args(args...));
}

export template <typename... T>
std::string translate(pqxx::connection& conn, const guild& guild, const std::string& key, T&&... args)
{
    pqxx::nontransaction txn(conn);
    return translate(txn, guild, key, args...);
}

}
