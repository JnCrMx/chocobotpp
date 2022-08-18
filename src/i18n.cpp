#include "i18n.hpp"

#include <map>
#include <fstream>
#include <spdlog/spdlog.h>

namespace chocobot::i18n {

std::map<std::string, std::map<std::string, std::string>> translations;
void init_i18n()
{
    std::ifstream langs("resources/languages.txt");
    std::string lang;
    while(std::getline(langs, lang))
    {
        auto& map = translations[lang];
        std::ifstream trans("resources/lang/"+lang+".lang");

        std::string line;
        while(std::getline(trans, line, '\n'))
        {
            if(line.empty()) continue;

            std::istringstream iss(line);

            std::string key, value;
            std::getline(iss, key, '=');
            std::getline(iss, value);

            map[key] = value;
            spdlog::trace("Loaded translation: {} -> \"{}\"", key, value);
        }
        spdlog::info("Loaded language {} with {} translations", lang, map.size());
    }
}

std::string translate_get(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
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

std::string translate_base(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
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

}