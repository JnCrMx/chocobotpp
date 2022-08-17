#include "i18n.hpp"

namespace chocobot::i18n {

std::string translate_get(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
{
    

    return key; // translation not found, just return the key
}

std::string translate_base(pqxx::transaction_base& txn, const guild& guild, const std::string& key)
{
    std::string translation = translate_get(txn, guild, key);
    return translation;
}

}