#pragma once

#include <fmt/core.h>
#include <fmt/chrono.h>
#include "database.hpp"

namespace chocobot::i18n {

inline std::string resource_root;
void init_i18n();

std::string translate_get(pqxx::transaction_base& txn, const guild& guild, const std::string& key);
std::string translate_base(pqxx::transaction_base& txn, const guild& guild, const std::string& key);

template <typename... T>
std::string translate(pqxx::transaction_base& txn, const guild& guild, const std::string& key, T&&... args)
{
    std::string translation = translate_base(txn, guild, key);
    return fmt::vformat(translation, fmt::make_format_args(args...));
}

template <typename... T>
std::string translate(pqxx::connection& conn, const guild& guild, const std::string& key, T&&... args)
{
    pqxx::nontransaction txn(conn);
    return translate(txn, guild, key, args...);
}

}
