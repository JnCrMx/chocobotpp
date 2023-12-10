#include "command.hpp"
#include "i18n.hpp"
#include "utils.hpp"
#include "branding.hpp"

#include <date/tz.h>

namespace chocobot {

class merry_christmas_command : public command
{
    public:
        merry_christmas_command() {}

        std::string get_name() override
        {
            return "merry-christmas";
        }

        dpp::coroutine<result> process_guild(pqxx::work& txn, database& db, const dpp::message_create_t& event, const guild& guild,
                dpp::snowflake receiver_guild, dpp::snowflake sender, dpp::snowflake receiver, int amount, std::string message) {
            spdlog::trace("merry-christmas: {} -> {} in {} ({} coins)", sender, receiver, receiver_guild, amount);

            int coins = db.get_coins(sender, receiver_guild, txn).value_or(0);
            if(coins < amount) {
                event.reply(utils::build_error(txn, guild, "merry-christmas.error.not_enough"));
                co_return result::user_error;
            }

            int year = static_cast<int>(
                std::chrono::year_month_day{
                    std::chrono::floor<std::chrono::days>(
                        std::chrono::system_clock::now())}.year());
            auto res = txn.exec_prepared0(insert_gift, receiver, sender, receiver_guild, amount,
                message.empty() ? std::optional<std::string>{std::nullopt} : message, year);
            if(res.affected_rows() != 1) {
                event.reply(utils::build_error(txn, guild, "merry-christmas.error.internal"));
                co_return result::system_error;
            }
            db.change_coins(sender, receiver_guild, -amount, txn);

            dpp::embed embed{};
            embed.set_title(i18n::translate(txn, guild, "merry-christmas.success.title"));
            embed.set_color(branding::colors::cookie);
            embed.set_description(i18n::translate(txn, guild, "merry-christmas.success.description"));
            event.reply(dpp::message(event.msg.channel_id, embed));

            txn.commit();

            co_return result::success;
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& connection, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            pqxx::work txn{connection};

            auto now = std::chrono::system_clock::now();
            auto now_zoned = date::make_zoned(guild.timezone, now);
            date::year_month_day ymd{date::floor<date::days>(now_zoned.get_local_time())};
            if(ymd.month() != date::December)
            {
                co_return result::user_error;
            }
            if(static_cast<unsigned int>(ymd.day()) >= 27) {
                event.reply(utils::build_error(txn, guild, "merry-christmas.error.late"));
                co_return result::user_error;
            }

            dpp::snowflake sender = event.msg.author.id;
            dpp::snowflake receiver;
            int amount;
            if(!(args >> receiver >> amount)) {
                event.reply(utils::build_error(txn, guild, "merry-christmas.error.narg"));
                co_return result::user_error;
            }
            if(amount < 0) {
                event.reply(utils::build_error(txn, guild, "merry-christmas.error.negative"));
                co_return result::user_error;
            }
            std::string message{std::istreambuf_iterator<char>(args), {}};

            auto receiver_user_res = (co_await discord.co_user_get_cached(receiver));
            if(receiver_user_res.is_error()) {
                event.reply(utils::build_error(txn, guild, "merry-christmas.error.who"));
                co_return result::user_error;
            }
            auto receiver_user = receiver_user_res.get<dpp::user_identified>();

            std::unordered_map<dpp::snowflake, std::pair<dpp::guild_member, dpp::guild_member>> possible_guilds;
            const auto& guild_map = (co_await discord.co_current_user_get_guilds()).get<dpp::guild_map>();
            for(const auto& [snowflake, g] : guild_map) {
                auto sender_res = (co_await discord.co_guild_get_member(g.id, sender));
                if(sender_res.is_error()) continue;
                auto receiver_res = (co_await discord.co_guild_get_member(g.id, receiver));
                if(receiver_res.is_error()) continue;

                possible_guilds.emplace(g.id, std::make_pair(
                    sender_res.get<dpp::guild_member>(),
                    receiver_res.get<dpp::guild_member>()));
            }
            if(possible_guilds.empty()) {
                event.reply(utils::build_error(txn, guild, "merry-christmas.error.mutual"));
                co_return result::user_error;
            }

            if(possible_guilds.size() == 1) {
                auto& [guild_id, _] = *possible_guilds.begin();
                co_return co_await process_guild(txn, db, event, guild,
                    guild_id, sender, receiver, amount, message);
            } else {
                dpp::embed embed{};
                embed.set_title(i18n::translate(txn, guild, "merry-christmas.server_select.title"));
                embed.set_color(branding::colors::cookie);
                embed.set_description(i18n::translate(txn, guild, "merry-christmas.server_select.description"));
                std::vector<dpp::snowflake> options{};
                for(const auto& [guild_id, members] : possible_guilds) {
                    const auto& [sender, receiver] = members;

                    int coins = db.get_coins(sender.user_id, sender.guild_id, txn).value_or(0);
                    if(coins >= amount) {
                        embed.add_field(std::string(branding::answer_emojis[options.size()])+" "+guild_map.at(guild_id).name,
                            i18n::translate(txn, guild, "merry-christmas.server_select.entry",
                                coins, utils::get_effective_name(receiver, receiver_user)),
                            false);
                        options.push_back(guild_id);
                    } else {
                        embed.add_field(guild_map.at(guild_id).name,
                            i18n::translate(txn, guild, "merry-christmas.server_select.entry.not_enough",
                                coins, utils::get_effective_name(receiver, receiver_user)),
                            false);
                    }
                }
                auto msg_id = (co_await discord.co_message_create(dpp::message(event.msg.channel_id, embed).set_reference(event.msg.id))).get<dpp::message>().id;
                for(size_t i=0; i<options.size(); i++) {
                    discord.message_add_reaction(msg_id, event.msg.channel_id, branding::answer_emojis[i]);
                }
                auto result = co_await dpp::when_any{
	                discord.on_message_reaction_add.when([&msg_id, &options, &sender](const dpp::message_reaction_add_t& e)->bool {
                        if(e.message_id != msg_id) return false;
                        dpp::snowflake reacting_user = nlohmann::json::parse(e.raw_event)["d"]["user_id"]; // for some reason dpp does not extract this field correctly... :/
                        if(reacting_user != sender) return false;

                        auto it = std::find(branding::answer_emojis.begin(), branding::answer_emojis.end(), e.reacting_emoji.name);
                        if(it == branding::answer_emojis.end()) return false;
                        int index = std::distance(branding::answer_emojis.begin(), it);
                        if(index >= options.size()) return false;

                        return true;
	                }),
	                event.from->creator->co_sleep(60)
	            };
                discord.message_delete(msg_id, event.msg.channel_id);

                if(result.index() == 0) {
                    auto& e = result.get<0>();
                    auto it = std::find(branding::answer_emojis.begin(), branding::answer_emojis.end(), e.reacting_emoji.name);
                    int index = std::distance(branding::answer_emojis.begin(), it);
                    co_return co_await process_guild(txn, db, event, guild,
                        options[index], sender, receiver, amount, message);
                } else {
                    event.reply(utils::build_error(txn, guild, "merry-christmas.error.timeout"));
                    co_return result::user_error;
                }
            }

            co_return result::success;
        }

        void prepare(chocobot&, database& db) override
        {
            insert_gift = db.prepare("insert_gift", "INSERT INTO christmas_presents(uid, sender, guild, amount, message, year) VALUES($1, $2, $3, $4, $5, $6)");
        }
    private:
        std::string insert_gift;

        static private_command_register<merry_christmas_command> reg;
};
private_command_register<merry_christmas_command> merry_christmas_command::reg{};

}
