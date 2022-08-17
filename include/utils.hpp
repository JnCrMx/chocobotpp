#pragma once

#include <dpp/dpp.h>
#include <pqxx/pqxx>

#include "database.hpp"

namespace chocobot::utils {

dpp::message build_error(pqxx::connection& db, const guild& guild, const std::string& key);

}