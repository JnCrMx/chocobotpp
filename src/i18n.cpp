#include "i18n.hpp"

namespace chocobot::i18n {

std::string translate_get(pqxx::connection& db, const guild& guild, const std::string& key)
{
    

    return key; // translation not found, just return the key
}

std::string translate_base(pqxx::connection& db, const guild& guild, const std::string& key)
{
    std::string translation = translate_get(db, guild, key);
    return translation;
}

}