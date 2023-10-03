#include "command.hpp"

#include <utils.hpp>
#include <i18n.hpp>
#include <branding.hpp>

#include <array>

namespace chocobot {

class mystats_command : public command
{
    public:
        mystats_command() {}

        struct title{std::string_view title;};
        struct empty_inline{};
        struct empty_line{};
        struct stat{std::string_view stat;};
        struct split_message{};
        using stat_line = std::variant<title, empty_inline, empty_line, stat, split_message>;
        static constexpr std::array STAT_LAYOUT = {
                stat_line{stat{"max_coins"}},
                stat_line{stat{"daily.max_streak"}},
            stat_line{empty_line{}},
            stat_line{title{"Quiz"}},
                stat_line{stat{"game.quiz.played"}},
                stat_line{stat{"game.quiz.sponsored"}},
                stat_line{stat{"game.quiz.won"}},
                stat_line{stat{"game.quiz.won.place.1"}},
                stat_line{stat{"game.quiz.won.place.2"}},
                stat_line{stat{"game.quiz.won.place.3"}},
            stat_line{empty_line{}},
            stat_line{title{"Geschenke"}},
                stat_line{stat{"game.geschenke.played"}},
                stat_line{stat{"game.geschenke.sponsored"}},
                stat_line{stat{"game.geschenke.collected"}},
            stat_line{split_message{}},

            stat_line{title{"Block"}},
                stat_line{stat{"game.block.played"}},
                stat_line{stat{"game.block.prize.1000"}},
                stat_line{stat{"game.block.prize.500"}},
                stat_line{stat{"game.block.prize.100"}},
                stat_line{stat{"game.block.prize.0"}},
                stat_line{stat{"game.block.prize.-20"}},
                stat_line{stat{"game.block.prize.-100"}},
                stat_line{stat{"game.block.prize.-250"}},
                stat_line{empty_inline{}},
            stat_line{empty_line{}},
            stat_line{title{"Wer ist es?"}},
			    stat_line{stat{"game.weristes.played"}},
			    stat_line{stat{"game.weristes.sponsored"}},
			    stat_line{stat{"game.weristes.won"}},
        };

        std::string get_name() override
        {
            return "mystats";
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& conn, database& db,
            dpp::cluster&, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            pqxx::nontransaction txn{conn};

            dpp::user target = event.msg.author;

            dpp::embed eb = new_embed(target, txn, guild);
            for(auto line : STAT_LAYOUT)
            {
                if(std::holds_alternative<empty_line>(line))
                {
                    eb.add_field("", "", false);
                }
                else if(std::holds_alternative<empty_inline>(line))
                {
                    eb.add_field("", "", true);
                }
                else if(std::holds_alternative<title>(line))
                {
                    eb.add_field(std::string{std::get<title>(line).title}, "", false);
                }
                else if(std::holds_alternative<split_message>(line))
                {
                    event.reply(dpp::message{dpp::snowflake{}, eb});
                    eb = new_embed(target, txn, guild);
                }
                else if(std::holds_alternative<stat>(line))
                {
                    std::string stat = std::string{std::get<struct stat>(line).stat};
                    int value = db.get_stat(target.id, guild.id, stat, txn).value_or(0);
                    eb.add_field(i18n::translate(txn, guild, "command.mystats.entry."+stat), std::to_string(value), true);
                }
            }
            event.reply(dpp::message{dpp::snowflake{}, eb});

            co_return result::success;
        }
    private:
        static dpp::embed new_embed(dpp::user user, pqxx::nontransaction& txn, const guild& guild)
        {
            dpp::embed eb{};
            eb.set_author(dpp::embed_author{user.format_username(), "", user.get_avatar_url(), ""});
            eb.set_color(branding::colors::cookie);
            eb.set_title(i18n::translate(txn, guild, "command.mystats.title"));
            return eb;
        }

        static command_register<mystats_command> reg;
};
command_register<mystats_command> mystats_command::reg{};

}
