#include <iostream>
#include <string>
#include <coroutine>
#include <random>
#include <set>
#include <iomanip>
#include <charconv>
#include <optional>

import chocobot;
import chocobot.i18n;
import chocobot.utils;

namespace chocobot {

class random_command : public command
{
    private:
        std::default_random_engine rng;
    public:
        random_command() {
            rng.seed(std::random_device{}());
        }

        std::string get_name() override
        {
            return "random";
        }

        dpp::coroutine<result> execute(chocobot& bot, pqxx::connection& conn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            if(!event.msg.mentions.empty())
            {
                std::uniform_int_distribution<int> dist(0, event.msg.mentions.size()-1);
                auto [user, member] = event.msg.mentions.at(dist(rng));

                event.reply(i18n::translate(conn, guild, "command.random.user", user.get_mention()));
                co_return result::success;
            }
            if(!event.msg.mention_roles.empty())
            {
                std::set<dpp::snowflake> roles(event.msg.mention_roles.begin(), event.msg.mention_roles.end());
                auto cb = co_await discord.co_guild_get_members(guild.id, 1000, 0ul);
                if(!cb.is_error()) {
                    auto map = cb.get<dpp::guild_member_map>();
                    std::vector<dpp::guild_member> members;
                    for(auto [_, member] : map)
                    {
                        for(auto role : member.get_roles())
                        {
                            if(roles.contains(role))
                            {
                                members.push_back(member);
                            }
                        }
                    }

                    std::uniform_int_distribution<int> dist(0, members.size()-1);
                    auto member = members.at(dist(rng));

                    auto conn = db.acquire_connection();
                    event.reply(i18n::translate(*conn, guild, "command.random.user", member.get_mention()));
                    co_return command::result::success;
                }
                co_return command::result::system_error;
            }
            if(event.msg.mention_everyone)
            {
                auto cb = co_await discord.co_guild_get_members(guild.id, 1000, 0ul);
                if(!cb.is_error()) {
                    auto map = cb.get<dpp::guild_member_map>();
                    std::vector<dpp::guild_member> members;
                    for(auto [_, m] : map)
                    {
                        members.push_back(m);
                    }

                    std::uniform_int_distribution<int> dist(0, members.size()-1);
                    auto member = members.at(dist(rng));

                    auto conn = db.acquire_connection();
                    event.reply(i18n::translate(*conn, guild, "command.random.user", member.get_mention()));
                    co_return command::result::success;
                }
                co_return command::result::system_error;
            }

            std::string arg1, arg2;
            args >> std::quoted(arg1) >> std::quoted(arg2);

            if(arg1.empty() || arg2.empty())
            {
                event.reply(utils::build_error(conn, guild, "command.random.error.unsupported"));
                co_return result::user_error;
            }

            auto to_int = [](std::string_view s) -> std::optional<intmax_t>
            {
                intmax_t value;
                auto [ptr, ec] = std::from_chars(s.begin(), s.end(), value);
                if(ec == std::errc{} && ptr == s.end())
                    return value;
                else
                    return std::nullopt;
            };
            auto a = to_int(arg1);
            auto b = to_int(arg2);
            if(a && b)
            {
                std::uniform_int_distribution<int> dist(*a, *b);
                event.reply(i18n::translate(conn, guild, "command.random.number", dist(rng)));
                co_return result::success;
            }
            else
            {
                std::vector<std::string> terms = {arg1, arg2};
                while(args.good())
                {
                    std::string s;
                    args >> std::quoted(s);
                    terms.push_back(s);
                }

                std::uniform_int_distribution<int> dist(0, terms.size()-1);
                std::string term = terms.at(dist(rng));

                if(term.contains("@everyone") || term.contains("@here"))
                {
                    if(!guild.operators.contains(event.msg.author.id))
                    {
                        event.reply(utils::build_error(conn, guild, "command.random.error.perm"));
                        co_return result::user_error;
                    }
                }
                event.reply(i18n::translate(conn, guild, "command.random.word", term));
                co_return result::success;
            }
        }
    private:
        static command_register<random_command> reg;
};
command_register<random_command> random_command::reg{};

}
