#include "database.hpp"

namespace chocobot {

void database::prepare()
{
    m_connection.prepare("get_coins", "SELECT coins FROM coins WHERE uid=$1 AND guild=$2");
    m_connection.prepare("change_coins", "UPDATE coins SET coins=coins+$3 WHERE uid=$1 AND guild=$2");
    m_connection.prepare("set_coins", "UPDATE coins SET coins=$3 WHERE uid=$1 AND guild=$2");
    m_connection.prepare("get_stat", "SELECT value FROM user_stats WHERE uid=$1 AND guild=$2 AND stat=$3");
    m_connection.prepare("set_stat", "INSERT INTO user_stats(uid, guild, stat, value) VALUES($1, $2, $3, $4) ON CONFLICT (uid, guild, stat) DO UPDATE SET value=$4");
}

}