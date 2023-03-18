#include "games.hpp"

#include <array>
#include <random>

namespace chocobot {

template<typename tuple_t>
constexpr static auto get_array_from_tuple(tuple_t&& tuple)
{
    constexpr auto get_array = [](auto&& ... x){ return std::array{std::forward<decltype(x)>(x) ... }; };
    return std::apply(get_array, std::forward<tuple_t>(tuple));
}

class slot_machine : public single_player_game
{
    public:
        using single_player_game::single_player_game;

        constexpr static std::string_view name = "slot-machine";
        const std::string get_name() override
        {
            return std::string(name);
        }

        int get_cost() override
        {
            return 10;
        }

    private:
        constexpr static std::array emoji_fruits  = {"🍎", "🍋", "🍉", "🍓", "🍈", "🍒", "🍍"};
        constexpr static std::array emoji_hearts  = {"❤️", "🧡", "💛", "💚", "💙", "💜", "🖤"};
        constexpr static std::array emoji_money   = {"💸", "💵", "💴", "💶", "💷", "💰", "💳"};
        constexpr static std::array emoji_other   = {"🎲", "🏆", "🎫", "🎮"};
        constexpr static std::array emoji_special = {"🎱", "💎", "🍪"};

        constexpr static std::array emoji_wheel = get_array_from_tuple(std::tuple_cat(emoji_fruits, emoji_hearts, emoji_money, emoji_other, emoji_special));

        enum class category
        {
            fruits,
            hearts,
            money,
            other,
            special
        };
        static constexpr category get_category(const std::string_view& emoji)
        {
            if(std::find(emoji_fruits .begin(), emoji_fruits .end(), emoji) != emoji_fruits .end()) return category::fruits;
            if(std::find(emoji_hearts .begin(), emoji_hearts .end(), emoji) != emoji_hearts .end()) return category::hearts;
            if(std::find(emoji_money  .begin(), emoji_money  .end(), emoji) != emoji_money  .end()) return category::money;
            if(std::find(emoji_other  .begin(), emoji_other  .end(), emoji) != emoji_other  .end()) return category::other;
            if(std::find(emoji_special.begin(), emoji_special.end(), emoji) != emoji_special.end()) return category::special;
            return category::other;
        }

        static constexpr std::array<std::array<std::string_view, 3>, 3> build_wheels(const std::array<int, 3>& wheels)
        {
            std::array<std::array<std::string_view, 3>, 3> array;

            array[0][0] = emoji_wheel[(wheels[0] + 1) % emoji_wheel.size()];
            array[0][1] = emoji_wheel[(wheels[0]    ) % emoji_wheel.size()];
            array[0][2] = emoji_wheel[(wheels[0] - 1) % emoji_wheel.size()];

            array[1][0] = emoji_wheel[(wheels[1] + 1) % emoji_wheel.size()];
            array[1][1] = emoji_wheel[(wheels[1]    ) % emoji_wheel.size()];
            array[1][2] = emoji_wheel[(wheels[1] - 1) % emoji_wheel.size()];

            array[2][0] = emoji_wheel[(wheels[2] + 1) % emoji_wheel.size()];
            array[2][1] = emoji_wheel[(wheels[2]    ) % emoji_wheel.size()];
            array[2][2] = emoji_wheel[(wheels[2] - 1) % emoji_wheel.size()];

            return array;
        }

        std::string build_message(const std::array<int, 3>& wheels, pqxx::transaction_base& txn, const std::string& key = "game.slot-machine.format.playing", int prize = 0)
        {
            auto w = build_wheels(wheels);
            return i18n::translate(txn, m_guild, key, m_host.format_username(),
                w[0][0], w[1][0], w[2][0],
                w[0][1], w[1][1], w[2][1],
                w[0][2], w[1][2], w[2][2],
                prize);
        }

        static constexpr int special_multiplier = 2;
        static constexpr int prize_three = 1000;
        static constexpr int prize_two_next = 100;
        static constexpr int prize_two = 50;
        static constexpr int prize_three_category = 25;
        static constexpr int prize_two_next_category = 15;
        static constexpr int prize_two_category = 10;

        static constexpr int special(category c)
        {
            return c == category::special ? special_multiplier : 1;
        }
        static constexpr int calculate_prize(const std::array<int, 3>& wheels)
        {
            auto w0 = emoji_wheel[wheels[0]]; auto c0 = get_category(w0);
            auto w1 = emoji_wheel[wheels[1]]; auto c1 = get_category(w1);
            auto w2 = emoji_wheel[wheels[2]]; auto c2 = get_category(w2);

            if(w0 == w1 && w0 == w2)
            {
                return prize_three * special(c0);
            }
            else if(w0 == w1 || w1 == w2)
            {
                return prize_two_next * special(c1);
            }
            else if(w0 == w2)
            {
                return prize_two * special(c0);
            }
            else if(c0 == c1 && c0 == c2)
            {
                return prize_three_category * special(c0);
            }
            else if(c0 == c1 || c1 == c2)
            {
                return prize_two_next_category * special(c1);
            }
            else if(c0 == c2)
            {
                return prize_two_category * special(c0);
            }
            else
            {
                return 0;
            }
        }

    public:
        void play() override
        {
            static std::random_device hwrng;
            static std::default_random_engine engine(hwrng());
            static std::uniform_int_distribution<int> dist(0, 2);
            std::array<int, 3> wheels{dist(engine), dist(engine), dist(engine)};

            dpp::message msg;
            {
                auto conn = m_db.acquire_connection();
                pqxx::nontransaction txn(*conn);
                std::string initial = build_message(wheels, txn);
                msg = {m_channel, initial};
            }
            msg = m_discord.message_create_sync(msg);
            m_discord.message_add_reaction(msg, "📍");
            m_reaction_handle = m_discord.on_message_reaction_add([this, &msg](const dpp::message_reaction_add_t& event){
                if(event.message_id != msg.id) return;
                if(event.reacting_user.id != m_host.id) return;
                if(event.reacting_emoji.name != "📍") return;

                std::unique_lock<std::mutex> guard(m_lock);
                m_halt = true;
                m_signal.notify_all();
            });
            {
                std::unique_lock<std::mutex> guard(m_lock);
                while(true)
                {
                    bool do_halt = m_signal.wait_for(guard, 1s, [this](){
                        return m_halt;
                    });
                    if(do_halt) break;

                    wheels[0] += 1; wheels[0] %= emoji_wheel.size();
                    wheels[1] += 2; wheels[1] %= emoji_wheel.size();
                    wheels[2] += 3; wheels[2] %= emoji_wheel.size();
                    {
                        auto conn = m_db.acquire_connection();
                        pqxx::nontransaction txn(*conn);
                        std::string content = build_message(wheels, txn);
                        msg.set_content(content);
                    }
                    m_discord.message_edit(msg);
                }
            }
            m_discord.on_message_reaction_add.detach(m_reaction_handle);

            int prize = calculate_prize(wheels);
            if(prize > 0)
            {
                auto conn = m_db.acquire_connection();
                pqxx::work txn(*conn);
                m_db.change_coins(m_host.id, m_guild.id, prize, txn);
                std::string content = build_message(wheels, txn, "game.slot-machine.format.won", prize);
                msg.set_content(content);
                txn.commit();
            }
            else
            {
                auto conn = m_db.acquire_connection();
                pqxx::nontransaction txn(*conn);
                std::string content = build_message(wheels, txn, "game.slot-machine.format.lost");
                msg.set_content(content);
            }
            m_discord.message_edit(msg);
            m_discord.message_delete_reaction_emoji(msg, "📍");
        }
    private:
        static command_register<game_command<slot_machine>> reg;

        bool m_halt = false;
        dpp::event_handle m_reaction_handle;
        std::mutex m_lock;
};
command_register<game_command<slot_machine>> slot_machine::reg{};

}
