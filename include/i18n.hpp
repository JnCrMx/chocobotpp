#pragma once

#include <fmt/core.h>
#include "database.hpp"

namespace chocobot {

std::string translate_get(pqxx::connection& db, const guild& guild, const std::string& key);
std::string translate_base(pqxx::connection& db, const guild& guild, const std::string& key);

template <typename... T>
std::string translate(pqxx::connection& db, const guild& guild, const std::string& key, T&&... args)
{
    std::string translation = translate_base(db, guild, key);
    return fmt::vformat(translation, fmt::make_format_args(args...));
}

}
