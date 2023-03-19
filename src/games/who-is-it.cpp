#include "games.hpp"

#include "utils.hpp"

#include <Magick++.h>
#include <random>

namespace chocobot {

class who_is_it : public multi_player_game
{
    public:
        using multi_player_game::multi_player_game;

        constexpr static std::string_view name = "wer-ist-es";
        const std::string get_name() override
        {
            return std::string(name);
        }

        constexpr static int cost = 100;
        int get_cost() override
        {
            return cost;
        }

    private:
        static constexpr int default_avatar_size = 256;
        std::optional<std::string> find_avatar_url(const dpp::guild_member& member, int size = default_avatar_size)
        {
            std::string avatar_url = member.get_avatar_url(size);
            if(!avatar_url.empty()) return avatar_url;

            dpp::user user = m_discord.user_get_cached_sync(member.user_id);
            if(!user.avatar.to_string().empty()) return user.get_avatar_url(size);

            return std::nullopt;
        }

        static Magick::Blob make_pixel_gif(const Magick::Image& avatar)
        {
            auto size = avatar.size();

            std::vector<Magick::Image> frames;
            for(double i=0.01; i<1.0; i*=1.21)
            {
                Magick::Image img = avatar;

                img.resize(Magick::Geometry{
                    (size_t)(((double)size.width())*i),
                    (size_t)(((double)size.height())*i)
                });
                img.filterType(Magick::FilterType::PointFilter);
                img.resize(size);
                
                img.animationDelay(100);
                img.magick("GIF");
                frames.push_back(img);
            }

            Magick::Blob blob{};
            Magick::writeImages(frames.begin(), frames.end(), &blob, true);
            return blob;
        }
        static Magick::Blob make_zoom_gif(const Magick::Image& avatar)
        {
            auto size = avatar.size();

            std::vector<Magick::Image> frames;
            for(double i=16; i>1; i/=1.0284)
            {
                double partWidth = size.width()/i;
                double partHeight = size.height()/i;
                double partX = size.width()/2.0 - partWidth/2.0;
                double partY = size.height()/2.0 - partHeight/2.0;

                size_t pw = partWidth; size_t ph = partHeight;
                ssize_t px = partX; ssize_t py = partY;

                Magick::Image img{Magick::Geometry{pw, ph}, Magick::Color("black")};
                img.copyPixels(avatar, Magick::Geometry{pw, ph, px, py}, Magick::Offset{0, 0});
                img.resize(size);
                
                img.animationDelay(25);
                img.magick("GIF");
                frames.push_back(img);
            }

            Magick::Blob blob{};
            Magick::writeImages(frames.begin(), frames.end(), &blob, true);
            return blob;
        }

        static constexpr std::array gif_generators = {
            &who_is_it::make_pixel_gif,
            &who_is_it::make_zoom_gif
        };

        static constexpr int max_avatar_tries = 25;
        static constexpr auto answer_timeout = 5s;
        static constexpr auto game_duration = 1min;
        static constexpr auto cleanup_time = 1min;

        static constexpr int base_reward = 150;
        static constexpr int additional_reward = 15;

    public:
        void play() override
        {
            auto members = m_discord.guild_get_members_sync(m_guild.id, 1000, dpp::snowflake{});

            static std::random_device hwrng;
            static std::default_random_engine engine(hwrng());
            std::uniform_int_distribution<int> dist(0, members.size()-1);

            std::optional<std::string> optional_avatar_url;
            int tries = 0;
            do
            {
                m_target_member = std::get<1>(*std::next(members.cbegin(), dist(engine)));
                optional_avatar_url = find_avatar_url(m_target_member);
                tries++;
                if(tries >= max_avatar_tries)
                {
                    connection_wrapper conn = m_db.acquire_connection();
                    pqxx::work txn(*conn);
                    m_discord.message_create(utils::build_error(txn, m_guild, "game.weristes.error.noent").set_channel_id(m_channel));
                    m_db.change_coins(m_host.id, m_guild.id, get_cost(), txn);
                    txn.commit();
                    return;
                }
            }
            while(!optional_avatar_url.has_value());

            std::string avatar_url = optional_avatar_url.value();
            m_target_user = m_discord.user_get_cached_sync(m_target_member.user_id);

            std::promise<std::string> avatar_promise{};
            m_discord.request(avatar_url, dpp::http_method::m_get, [&avatar_promise](const dpp::http_request_completion_t& event){
                try
                {
                    if(event.error != dpp::http_error::h_success)
                    {
                        throw std::runtime_error("http request failed with status "+std::to_string(event.status));
                    }
                    avatar_promise.set_value(event.body);
                }
                catch(...)
                {
                    avatar_promise.set_exception(std::current_exception());
                }
            });
            std::string avatar_data = avatar_promise.get_future().get();
            Magick::Blob avatar_blob{avatar_data.data(), avatar_data.size()};

            Magick::Image avatar{};
            avatar.read(avatar_blob);

            static std::uniform_int_distribution<int> gif_type_dist(0, gif_generators.size()-1);
            int generator = gif_type_dist(engine);
            Magick::Blob gif = gif_generators[generator](avatar);

            dpp::embed eb{};
            {
                auto conn = m_db.acquire_connection();
                pqxx::nontransaction txn{*conn};
                eb.set_title(i18n::translate(txn, m_guild, "game.weristes.title"));
                eb.set_description(i18n::translate(txn, m_guild, "game.weristes.description"));
                eb.set_color(branding::colors::game);
            }
            m_message = m_discord.message_create_sync(
                dpp::message{m_channel, eb}
                    .add_file("image.gif", std::string{(char*)gif.data(), gif.length()})
            );

            m_message_handle = m_discord.on_message_create([this](const dpp::message_create_t& event){
                if(event.msg.channel_id != m_channel) return;

                auto id = event.msg.author.id;
                if(!m_players.contains(id)) return;

                auto now = std::chrono::system_clock::now();
                if(m_timeouts.contains(id))
                {
                    auto last_time = m_timeouts[id];
                    if((now - last_time) < answer_timeout)
                    {
                        m_discord.message_add_reaction(event.msg, "⏳");
                        return;
                    }
                }

                std::string text = event.msg.content;
                if(text == m_target_user.username || text == m_target_user.format_username() ||
                   text == std::to_string(m_target_user.id) ||
                   (!m_target_member.nickname.empty() && text == m_target_member.nickname))
                {
                    m_discord.message_add_reaction(event.msg, "✅");

                    std::scoped_lock<std::mutex> sl{m_lock};
                    m_winner = event.msg.author;
                    m_signal.notify_all();
                }
                else
                {
                    m_timeouts[id] = now;
                    m_discord.message_add_reaction(event.msg, "❌");
                }
            });

            {
                std::unique_lock<std::mutex> guard(m_lock);
                bool won = m_signal.wait_for(guard, game_duration, [this](){
                    return m_winner.has_value();
                });

                m_discord.on_message_create.detach(m_message_handle);

                auto conn = m_db.acquire_connection();
                if(won)
                {
                    pqxx::work txn{*conn};

                    dpp::embed eb{};
                    eb.set_title(i18n::translate(txn, m_guild, "game.weristes.results.title"));
                    eb.set_description(i18n::translate(txn, m_guild, "game.weristes.results.guessed",
                        m_winner->format_username(), m_target_user.format_username()));
                    eb.set_color(branding::colors::game);
                    eb.set_thumbnail(avatar_url);

                    int reward = base_reward + (m_players.size()-1) * additional_reward;
                    eb.add_field(m_winner->format_username(), i18n::translate(txn, m_guild, "game.weristes.results.won", reward));
                    if(m_winner == m_host)
                    {
                        m_db.change_coins(m_winner->id, m_guild.id, reward + get_cost(), txn);
                    }
                    else
                    {
                        m_db.change_coins(m_winner->id, m_guild.id, reward, txn);
                        eb.add_field(m_host.format_username(), i18n::translate(txn, m_guild, "game.weristes.results.lost", get_cost()));
                    }

                    m_discord.message_create(dpp::message{m_channel, eb});

                    txn.commit();
                }
                else
                {
                    pqxx::nontransaction txn{*conn};
                    
                    dpp::embed eb{};
                    eb.set_title(i18n::translate(txn, m_guild, "game.weristes.results.title"));
                    eb.set_description(i18n::translate(txn, m_guild, "game.weristes.results.noone",
                        m_target_user.format_username()));
                    eb.set_color(branding::colors::game);
                    eb.set_thumbnail(avatar_url);

                    eb.add_field(m_host.format_username(), i18n::translate(txn, m_guild, "game.weristes.results.lost", get_cost()));

                    m_discord.message_create(dpp::message{m_channel, eb});
                }
            }
            std::this_thread::sleep_for(cleanup_time);
            m_discord.message_delete(m_message.id, m_channel);
        }
    private:
        dpp::guild_member m_target_member;
        dpp::user m_target_user;

        dpp::message m_message;
        dpp::event_handle m_message_handle;

        std::map<dpp::snowflake, std::chrono::system_clock::time_point> m_timeouts;

        std::optional<dpp::user> m_winner;


        static command_register<game_command<who_is_it>> reg;
};
command_register<game_command<who_is_it>> who_is_it::reg{};

}
