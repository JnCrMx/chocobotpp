#include "command.hpp"
#include "branding.hpp"
#include "database.hpp"
#include "i18n.hpp"
#include "utils.hpp"

#include <dpp/coro.h>
#include <pqxx/pqxx>

#include <unordered_map>

namespace chocobot {

class shop_command : public command
{
    private:
        std::string list_statement;
        std::string inventory_statement;
        std::string role_statement;
        std::string test_statement;
        std::string buy_statement;
        std::string inventory_get_statement;
    public:
        shop_command() {}

        std::string get_name() override
        {
            return "shop";
        }

        dpp::coroutine<result> execute(chocobot& bot, pqxx::connection& conn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            std::string subcommand;
            args >> subcommand;

            dpp::snowflake user_id = event.msg.author.id;

            if(subcommand.empty() || subcommand == "l" || subcommand == "list") {
                pqxx::nontransaction txn{conn};
                dpp::embed eb = co_await show_list(discord, txn, guild, "list", txn.exec_prepared(list_statement, guild.id));
                event.reply(dpp::message({}, eb));
                co_return result::success;
            }
            else if(subcommand == "i" || subcommand == "inventory") {
                pqxx::nontransaction txn{conn};
                dpp::embed eb = co_await show_list(discord, txn, guild, "inventory", txn.exec_prepared(inventory_statement, guild.id, user_id));
                event.reply(dpp::message({}, eb));
                co_return result::success;
            }
            else if(subcommand == "b" || subcommand == "buy") {
                std::string alias;
                args >> alias;

                pqxx::work txn{conn};
                auto role_result = txn.exec_prepared(role_statement, guild.id, alias);
                if(role_result.empty()) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.buy.noent"));
                    co_return command::result::user_error;
                }
                auto [role_id, cost] = role_result.front().as<dpp::snowflake, int>();

                auto test_result = txn.exec_prepared(test_statement, role_id, user_id);
                if(!test_result.empty()) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.buy.dup"));
                    co_return command::result::user_error;
                }

                if(db.get_coins(user_id, guild.id, txn).value_or(0) < cost) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.buy.not_enough"));
                    co_return command::result::user_error;
                }

                auto buy_result = txn.exec_prepared0(buy_statement, guild.id, user_id, role_id);
                if(buy_result.affected_rows() == 0) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.internal"));
                    co_return command::result::system_error;
                }
                db.change_coins(user_id, guild.id, -cost, txn);

                dpp::embed eb{};
                eb.set_color(branding::colors::coins);
                eb.set_title(i18n::translate(txn, guild, "command.shop.buy.title"));
                eb.set_description(i18n::translate(txn, guild, "command.shop.buy.description", alias, cost));
                eb.set_footer(i18n::translate(txn, guild, "command.shop.buy.footer", guild.prefix, alias), {});
                event.reply(dpp::message({}, eb));

                txn.commit();
            }
            else if(subcommand == "a" || subcommand == "activate") {
                std::string alias;
                args >> alias;

                pqxx::nontransaction txn{conn};
                auto res = txn.exec_prepared(inventory_get_statement, guild.id, user_id, alias);

                if(res.empty()) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.activate.not_owned"));
                    co_return command::result::user_error;
                }

                auto role = res.front().front().as<dpp::snowflake>();
                auto member = (co_await discord.co_guild_get_member(guild.id, user_id)).get<dpp::guild_member>();
                if(std::find(member.roles.begin(), member.roles.end(), role) != member.roles.end()) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.activate.dup"));
                    co_return command::result::user_error;
                }

                auto role_result = co_await discord.co_guild_member_add_role(guild.id, user_id, role);
                if(role_result.is_error() || !role_result.get<dpp::confirmation>().success) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.internal"));
                    co_return command::result::user_error;
                }

                dpp::embed eb{};
                eb.set_color(branding::colors::coins);
                eb.set_title(i18n::translate(txn, guild, "command.shop.activate.title"));
                eb.set_description(i18n::translate(txn, guild, "command.shop.activate.description", alias));
                eb.set_footer(i18n::translate(txn, guild, "command.shop.activate.footer", guild.prefix, alias), {});
                event.reply(dpp::message({}, eb));
            }
            else if(subcommand == "d" || subcommand == "deactivate") {
                std::string alias;
                args >> alias;

                pqxx::nontransaction txn{conn};
                auto res = txn.exec_prepared(inventory_get_statement, guild.id, user_id, alias);

                if(res.empty()) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.deactivate.not_owned"));
                    co_return command::result::user_error;
                }

                auto role = res.front().front().as<dpp::snowflake>();
                auto member = (co_await discord.co_guild_get_member(guild.id, user_id)).get<dpp::guild_member>();
                if(std::find(member.roles.begin(), member.roles.end(), role) == member.roles.end()) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.deactivate.dup"));
                    co_return command::result::user_error;
                }

                auto role_result = co_await discord.co_guild_member_remove_role(guild.id, user_id, role);
                if(role_result.is_error() || !role_result.get<dpp::confirmation>().success) {
                    event.reply(utils::build_error(txn, guild, "command.shop.error.internal"));
                    co_return command::result::user_error;
                }

                dpp::embed eb{};
                eb.set_color(branding::colors::coins);
                eb.set_title(i18n::translate(txn, guild, "command.shop.deactivate.title"));
                eb.set_description(i18n::translate(txn, guild, "command.shop.deactivate.description", alias));
                eb.set_footer(i18n::translate(txn, guild, "command.shop.deactivate.footer", guild.prefix, alias), {});
                event.reply(dpp::message({}, eb));
            }

            co_return result::user_error;
        }

		void prepare(chocobot&, database& db) override
        {
            list_statement = db.prepare("list_shop", "SELECT role, alias, description, cost FROM shop_roles WHERE guild = $1");
            inventory_statement = db.prepare("shop_inventory", "SELECT r.role, r.alias, r.description, r.cost FROM shop_roles r, shop_inventory i WHERE r.guild = $1 AND r.guild = i.guild AND r.role = i.role AND i.user = $2");
            role_statement = db.prepare("find_shop_role", "SELECT role, cost FROM shop_roles WHERE guild = $1 AND alias = $2");
            test_statement = db.prepare("test_shop_role", "SELECT 1 FROM shop_inventory WHERE role = $1 AND \"user\" = $2");
            buy_statement = db.prepare("buy_shop_role", "INSERT INTO shop_inventory (guild, \"user\", role) VALUES ($1, $2, $3)");
            inventory_get_statement = db.prepare("shop_inventory_get", "SELECT r.role FROM shop_roles r, shop_inventory i WHERE r.guild = $1 AND r.guild = i.guild AND r.role = i.role AND i.user = $2 AND r.alias = $3");
        }
    private:
        static dpp::coroutine<dpp::embed> show_list(dpp::cluster& discord, pqxx::nontransaction& txn, const guild& guild,
            const std::string& type, pqxx::result&& result)
        {
            dpp::embed eb{};
            eb.set_color(branding::colors::coins);
            eb.set_title(i18n::translate(txn, guild, "command.shop."+type+".title"));
            eb.set_description(i18n::translate(txn, guild, "command.shop."+type+".description"));
            eb.set_footer(i18n::translate(txn, guild, "command.shop."+type+".footer", guild.prefix), {});

            auto roles = (co_await discord.co_roles_get(guild.id)).get<std::unordered_map<dpp::snowflake, dpp::role>>();
            for(const auto& row : result)
            {
                const auto& [role_id, alias, description, cost] = row.as<dpp::snowflake, std::string, std::string, int>();
                const auto& role = roles.at(role_id);

                eb.add_field(
                    i18n::translate(txn, guild, "command.shop."+type+".entry.key", alias, role.name),
                    i18n::translate(txn, guild, "command.shop."+type+".entry.value", description, cost),
                    false);
            }
            co_return eb;
        }

        static command_register<shop_command> reg;
};
command_register<shop_command> shop_command::reg{};

}
