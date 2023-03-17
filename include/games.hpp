#pragma once

#include "chocobot.hpp"
#include "database.hpp"
#include "branding.hpp"
#include "i18n.hpp"
#include "command.hpp"

#include <condition_variable>
#include <dpp/dpp.h>
#include <functional>
#include <mutex>
#include <vector>
#include <chrono>
#include <spdlog/spdlog.h>

#ifdef __unix__
#include <pthread.h>
#endif

namespace chocobot {

enum class game_state
{
    confirm,
    announce,
    running,
    finished,
    cancelled
};

using namespace std::chrono_literals;
class game
{
    public:
        game(chocobot& bot, database& db, dpp::cluster& discord, dpp::user host, guild guild, dpp::snowflake channel, bool no_confirm = false) :
            m_bot(bot),  m_db(db), m_discord(discord), m_host(host), m_guild(guild), m_channel(channel), m_no_confirm(no_confirm)
        {}
        virtual ~game() = default;

        virtual int get_cost() = 0;
        virtual const std::string get_name() = 0;

        void start()
        {
            m_thread = std::thread([this](){
                confirm();
            });
#ifdef __unix__
            pthread_setname_np(m_thread.native_handle(), ("Game "+get_name()).c_str());
#endif
        }
    protected:
        game_state m_state;
        chocobot& m_bot;
        database& m_db;
        dpp::cluster& m_discord;
        dpp::user m_host;
        guild m_guild;
        dpp::snowflake m_channel;
        bool m_no_confirm;

        std::mutex m_lock;
        std::condition_variable m_signal;

        virtual void confirm()
        {
            spdlog::debug("Confirming game {} by user {} in guild {}", get_name(), m_host.format_username(), m_guild.id);

            m_state = game_state::confirm;
            if(m_no_confirm)
            {
                announce();
            }

            dpp::embed embed{};
            {
                connection_wrapper conn = m_db.acquire_connection();
                pqxx::nontransaction txn{*conn};
                embed.set_color(branding::colors::game);
                embed.set_title(i18n::translate(txn, m_guild, "game.confirm.title"));
                embed.set_description(i18n::translate(txn, m_guild, "game.confirm.description"));
                embed.add_field(i18n::translate(txn, m_guild, "game.confirm.cost.key"),
                    i18n::translate(txn, m_guild, "game.confirm.cost.value", get_cost()), false);
            }
            m_confirm_message = m_discord.message_create_sync(dpp::message(m_channel, embed));
            m_reaction_handle = m_discord.on_message_reaction_add([this](const dpp::message_reaction_add_t& event){
                if(event.message_id != m_confirm_message.id) return;
                if(event.reacting_user.id != m_host.id) return;
                if(event.reacting_emoji.name != "✅") return;

                std::unique_lock<std::mutex> guard(m_lock);
                m_state = game_state::announce;
                m_signal.notify_all();
            });
            m_discord.message_add_reaction(m_confirm_message, "✅");

            bool confirmed;
            {
                std::unique_lock<std::mutex> guard(m_lock);
                confirmed = m_signal.wait_for(guard, 10s, [this](){
                    return m_state != game_state::confirm;
                });
            }
            m_discord.on_message_reaction_add.detach(m_reaction_handle);
            m_discord.message_delete(m_confirm_message.id, m_confirm_message.channel_id);
            if(confirmed)
            {
                spdlog::debug("Confirmed game {} by user {} in guild {}", get_name(), m_host.format_username(), m_guild.id);
                {
                    connection_wrapper conn = m_db.acquire_connection();
                    pqxx::work txn{*conn};
                    m_db.change_coins(m_host.id, m_guild.id, -get_cost(), txn);
                    txn.commit();
                }
                announce();
            }
            else
            {
                cancel();
            }
        }
        virtual void announce() = 0;
        virtual void cancel()
        {
            spdlog::debug("Cancelling game {} by user {} in guild {}", get_name(), m_host.format_username(), m_guild.id);
            m_state = game_state::cancelled;
            dpp::embed embed{};
            {
                connection_wrapper conn = m_db.acquire_connection();
                pqxx::nontransaction txn{*conn};
                embed.set_color(branding::colors::game);
                embed.set_title(i18n::translate(txn, m_guild, "game.cancel.title"));
                embed.set_description(i18n::translate(txn, m_guild, "game.cancel.description", m_host.format_username()));
            }
            m_discord.message_create(dpp::message(m_channel, embed));

            cleanup();
        }

        virtual void cleanup()
        {
            if(m_state == game_state::confirm)
            {
                m_discord.message_delete(m_confirm_message.id, m_confirm_message.channel_id);
            }
            m_state = game_state::finished;
            spdlog::debug("Cleaned up game {} by user {} in guild {}", get_name(), m_host.format_username(), m_guild.id);

            m_thread.detach();
            delete this;
        }
    private:
        std::thread m_thread;

        dpp::event_handle m_reaction_handle;
        dpp::message m_confirm_message;
};

template<class Game>
class game_command : public command
{
    public:
        game_command() {}

        std::string get_name() override
        {
            return std::string(Game::name);
        }

        result execute(chocobot& bot, pqxx::connection&, database& db, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream&) override
        {
            Game* g = new Game(bot, db, discord, event.msg.author, guild, event.msg.channel_id);
            g->start();
            return command::result::deferred;
        }
};

class single_player_game : public game
{
    public:
        using game::game;
        virtual void play() = 0;

        void announce() override
        {
            m_state = game_state::running;
            try
            {
                play();
            }
            catch(const std::exception& ex)
            {
                spdlog::error("Game {} hosted by {} failed with exception: {}", get_name(), m_host.id, ex.what());
            }
            cleanup();
        }
};

class multi_player_game : public game
{
    private:
        dpp::event_handle m_reaction_add_handle, m_reaction_remove_handle;
        dpp::message m_announce_message;
    protected:
        std::map<dpp::snowflake, dpp::user> m_players;
    public:
        using game::game;
        virtual void play() = 0;

        void announce() override
        {
            m_state = game_state::announce;
            m_players.emplace(m_host.id, m_host);

            dpp::embed embed{};
            {
                connection_wrapper conn = m_db.acquire_connection();
                pqxx::nontransaction txn{*conn};
                embed.set_color(branding::colors::game);
                embed.set_title(i18n::translate(txn, m_guild, "game.announce.title"));
                embed.set_description(i18n::translate(txn, m_guild, "game.announce.description", get_name()));
                embed.set_footer(i18n::translate(txn, m_guild, "game.announce.footer"), "");
            }

            m_announce_message = m_discord.message_create_sync(dpp::message(m_channel, embed));
            m_reaction_add_handle = m_discord.on_message_reaction_add([this](const dpp::message_reaction_add_t& event){
                if(event.message_id != m_announce_message.id) return;
                if(event.reacting_emoji.name != "✅") return;

                std::unique_lock<std::mutex> guard(m_lock);
                m_players.emplace(event.reacting_user.id, event.reacting_user);
            });
            m_reaction_remove_handle = m_discord.on_message_reaction_remove([this](const dpp::message_reaction_remove_t& event){
                if(event.message_id != m_announce_message.id) return;
                if(event.reacting_emoji.name != "✅") return;

                std::unique_lock<std::mutex> guard(m_lock);
                m_players.erase(event.reacting_user_id);
            });
            m_discord.message_add_reaction(m_announce_message, "✅");

            std::this_thread::sleep_for(10s);

            m_discord.on_message_reaction_add.detach(m_reaction_add_handle);
            m_discord.on_message_reaction_remove.detach(m_reaction_remove_handle);
            m_discord.message_delete(m_announce_message.id, m_announce_message.channel_id);

            m_state = game_state::running;
            try
            {
                play();
            }
            catch(const std::exception& ex)
            {
                spdlog::error("Game {} hosted by {} failed with exception: {}", get_name(), m_host.id, ex.what());
            }
            cleanup();
        }
};

}
