#include "games.hpp"
#include "i18n.hpp"
#include "branding.hpp"

#include <random>
#include <ranges>

namespace chocobot {

class quiz : public multi_player_game
{
    public:
        using multi_player_game::multi_player_game;

        constexpr static std::string_view name = "quiz";
        const std::string get_name() override
        {
            return std::string(name);
        }

        int get_cost() override
        {
            return 100;
        }

    private:
        static constexpr int winner_reward = 50;
        static constexpr int bonus_reward = 10;

        static constexpr auto game_duration = 15s;
        static constexpr auto cleanup_time = 1min;
        static constexpr std::array answer_emojis = {"1️⃣", "2️⃣", "3️⃣", "4️⃣", "5️⃣", "6️⃣", "7️⃣", "8️⃣", "9️⃣"};

        struct quiz_question
        {
            std::string question;
            std::vector<std::string> answers{};
            std::string source = "";
        };
        static std::vector<quiz_question> questions;

    public:
        void play() override
        {
            static std::random_device hwrng;
            static std::default_random_engine engine(hwrng());
            static std::uniform_int_distribution<int> dist(0, questions.size()-1);

            m_question = questions[dist(engine)];
            m_shuffle.resize(m_question.answers.size());
            std::iota(m_shuffle.begin(), m_shuffle.end(), 0);
            std::shuffle(m_shuffle.begin(), m_shuffle.end(), engine);

            dpp::embed eb{};
            eb.set_color(branding::colors::game);
            {
                auto conn = m_db.acquire_connection();
                eb.set_title(i18n::translate(*conn, m_guild, "game.quiz.title"));
            }
            eb.set_description(m_question.question);
            for(size_t i=0; i<m_question.answers.size(); i++)
            {
                eb.add_field(answer_emojis[i], m_question.answers[m_shuffle[i]], true);
            }

            m_quiz_message = m_discord.message_create_sync(dpp::message{m_channel, eb});
            m_reaction_handle = m_discord.on_message_reaction_add([this](const dpp::message_reaction_add_t& event){
                if(event.message_id != m_quiz_message.id) return;
                if(event.reacting_user.is_bot()) return;

                auto id = event.reacting_user.id;
                if(!m_players.contains(id)) return;

                auto it = std::find(answer_emojis.cbegin(), answer_emojis.cend(), event.reacting_emoji.name);
                if(it == answer_emojis.end()) return;

                int i = std::distance(answer_emojis.cbegin(), it);
                if(i >= m_shuffle.size()) return;

                m_selected_answers[id] = m_shuffle[i];
                m_answer_times[id] = std::chrono::system_clock::now();
            });
            for(size_t i=0; i<m_question.answers.size(); i++)
            {
                m_discord.message_add_reaction(m_quiz_message, answer_emojis[i]);
            }

            std::this_thread::sleep_for(game_duration);
            m_discord.on_message_create.detach(m_reaction_handle);

            std::vector<dpp::snowflake> winners;
            winners.reserve(m_selected_answers.size()/2);
            for(const auto& [k, v] : m_selected_answers)
            {
                if(v == 0)
                {
                    winners.push_back(k);
                }
            }
            std::sort(winners.begin(), winners.end(), [this](const dpp::snowflake& a, const dpp::snowflake& b){
                return m_answer_times[a] < m_answer_times[b];
            });
            {
                auto conn = m_db.acquire_connection();
                pqxx::work txn(*conn);

                dpp::embed eb{};
                eb.set_color(branding::colors::game);
                eb.set_title(i18n::translate(txn, m_guild, "game.quiz.results.title"));
                eb.set_description(i18n::translate(txn, m_guild,
                    winners.empty() ? "game.quiz.results.no_correct" : "game.quiz.results.description"));

                for(int i=0; i<winners.size(); i++)
                {
                    int reward = winner_reward + bonus_reward * (m_players.size()-1-i);
                    auto user = m_discord.user_get_cached_sync(winners[i]);

                    eb.add_field(user.format_username(),
                                i18n::translate(txn, m_guild, "game.quiz.results.won", reward));

                    if(user == m_host)
                    {
                        reward += get_cost();
                    }
                    m_db.change_coins(user.id, m_guild.id, reward, txn);
                }
                if(std::find(winners.begin(), winners.end(), m_host.id) == winners.end())
                {
                    eb.add_field(m_host.format_username(), i18n::translate(txn, m_guild, "game.quiz.results.lost", get_cost()));
                }
                eb.set_footer(i18n::translate(txn, m_guild, "game.quiz.results.footer", m_question.question, m_question.answers.at(0)), m_question.source);
                m_discord.message_create(dpp::message{m_channel, eb});

                txn.commit();
            }

            std::this_thread::sleep_for(cleanup_time);
            m_discord.message_delete(m_quiz_message.id, m_channel);
        }

        static void prepare(chocobot& bot, database& db)
        {
            auto path = i18n::resource_root+"/quiz.txt";
            std::ifstream quiz(path);
            if(!quiz.good())
            {
                spdlog::error("Cannot load questions for game \"{}\": File \"{}\" not found or not readable.", name, path);
                return;
            }
            
            quiz_question qq{};
            std::getline(quiz, qq.question);
            if(qq.question.empty())
            {
                spdlog::error("Cannot load questions for game \"{}\": File \"{}\" seems to be empty.", name, path);
                return;
            }

            std::string answer;
            while(std::getline(quiz, answer))
            {
                constexpr std::string_view source_prefix = "\t@";
                constexpr std::string_view answer_prefix = "\t";
                if(answer.starts_with(source_prefix))
                {
                    qq.source = answer.substr(source_prefix.size());
                }
                else if(answer.starts_with(answer_prefix))
                {
                    qq.answers.push_back(answer.substr(answer_prefix.size()));
                }
                else
                {
                    questions.push_back(qq);
                    spdlog::trace("Loaded question \"{}\" with {} answers.", qq.question, qq.answers.size());
                    qq = {};
                    qq.question = answer;
                }
            }
            questions.push_back(qq);
            spdlog::trace("Loaded question \"{}\" with {} answers.", qq.question, qq.answers.size());

            spdlog::debug("Loaded {} questions for game \"{}\".", questions.size(), name);
        }

    private:
        quiz_question m_question;
        std::vector<decltype(std::declval<quiz_question>().answers)::size_type> m_shuffle{};

        dpp::message m_quiz_message;
        dpp::event_handle m_reaction_handle;
        std::map<dpp::snowflake, int> m_selected_answers{};
        std::map<dpp::snowflake, std::chrono::system_clock::time_point> m_answer_times{};

        static command_register<game_command<quiz>> reg;
};
command_register<game_command<quiz>> quiz::reg{};
std::vector<quiz::quiz_question> quiz::questions{};

}
