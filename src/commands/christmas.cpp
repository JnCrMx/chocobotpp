#include "branding.hpp"
#include "command.hpp"
#include "i18n.hpp"
#include "utils.hpp"

#include <Magick++.h>
#include <pqxx/result>
#include <random>
#include <ranges>

namespace chocobot {

class christmas_command : public command
{
    private:
        struct gift {
            int id;
            dpp::snowflake sender;
            bool opened;
        };
        struct gift_image {
            std::string_view image;
            bool opened;
            float text_size;
            double text_rotation;
            unsigned int text_x;
            unsigned int text_y;
            unsigned int avatar_x;
            unsigned int avatar_y;
            unsigned int avatar_size;
        };
        static constexpr std::array unopened_gift_images = {
            gift_image("/gifts/Geschenk_blau_lila.png", false, 32.0f, -50.0, 195, 83, 120, 220, 128),
			gift_image("/gifts/Geschenk_blau_rot.png", false, 32.0f, -95.0, 87, 85, 95, 335, 140),
			gift_image("/gifts/Geschenk_cyan_rosa.png", false, 32.0f, 55.0, 143, 73, 70, 110, 75),
			gift_image("/gifts/Geschenk_gelb_grun.png", false, 32.0f, -100.0, 135, 66, 115, 202, 64),
			gift_image("/gifts/Geschenk_grun_blau.png", false, 32.0f, -65.0, 215, 90, 127, 277, 160),
			gift_image("/gifts/Geschenk_grun_rot.png", false, 32.0f, -90.0, 97, 80, 140, 238, 80),
			gift_image("/gifts/Geschenk_orange_indigo.png", false, 32.0f, -152.0, 95, 55, 190, 226, 80),
			gift_image("/gifts/Geschenk_orange_schwarz.png", false, 32.0f, -90.0, 100, 85, 100, 260, 128),
			gift_image("/gifts/Geschenk_pink_cyan.png", false, 32.0f, -110.0, 112, 75, 123, 225, 100),
			gift_image("/gifts/Geschenk_rot_wei.png", false, 32.0f, -120.0, 62, 65, 150, 238, 85)
        };
        static constexpr std::array<gift_image, 0> opened_gift_images{};
        static constexpr std::string_view christmas_tree = "/Tannenbaum.png";
        static constexpr unsigned int star_x = 922, star_y = 97, star_size = 96;

        static constexpr int bot_gift_amount = 250;
        static constexpr std::string_view bot_gift_message = "Merry Christmas from ChocoBot! 💝";

        struct {
            std::string get_gift;
            std::string open_gift;
            std::string list_gifts;
            std::string bot_gift;
        } statements;
    public:
        christmas_command() {}

        std::string get_name() override
        {
            return "christmas";
        }

        dpp::coroutine<result> show_gifts(pqxx::work& txn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, dpp::snowflake uid)
        {
            std::vector<gift> gifts{};
            {
                int year = static_cast<int>(std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())}.year());
                auto res = txn.exec_prepared(statements.list_gifts, uid, guild.id, year);
                for(const auto& row : res) {
                    gifts.push_back(gift{
                        row[0].as<int>(),
                        row[1].as<dpp::snowflake>(),
                        row[2].as<bool>()
                    });
                }
                if(std::find_if(gifts.begin(), gifts.end(), [&discord](const gift& g) { return g.sender == discord.me.id; }) == gifts.end()) {
                    auto res = txn.exec_prepared1(statements.bot_gift, uid, discord.me.id, guild.id, bot_gift_amount, bot_gift_message, year);
                    int id = res.front().as<int>();
                    gifts.push_back(gift{id, discord.me.id, false});
                    txn.commit();
                }
            }
            std::vector<gift> giftList{}; std::copy_if(gifts.begin(), gifts.end(), std::back_inserter(giftList), [](const gift& g) { return !g.opened; });
            std::vector<gift> opened{}; std::copy_if(gifts.begin(), gifts.end(), std::back_inserter(opened), [](const gift& g) { return g.opened; });

            static std::random_device hwrng;
            static std::default_random_engine engine(hwrng());
            std::shuffle(giftList.begin(), giftList.end(), engine);
            std::shuffle(opened.begin(), opened.end(), engine);
            std::copy(opened.begin(), opened.end(), std::back_inserter(giftList));

            spdlog::trace("giftList: {}", giftList.size());

            Magick::Image image{};
            image.read(i18n::resource_root+"/images"+std::string(christmas_tree));

            Magick::Image avatar{};
            {
                auto res = co_await discord.co_request(utils::get_effective_avatar_url(event.msg.member, event.msg.author), dpp::http_method::m_get);
                const auto& img = res.body;
                Magick::Blob inBlob{img.data(), img.size()};
                avatar.read(inBlob);
            }
            Magick::Image mask(avatar.size(), Magick::Color("transparent"));
            mask.draw(Magick::DrawableList{
                Magick::DrawableFillColor(Magick::Color("white")),
                Magick::DrawableCircle(mask.columns()/2.0, mask.rows()/2.0, 0, mask.rows()/2.0)
            });
            avatar.composite(mask, 0, 0, Magick::CopyOpacityCompositeOp);

            image.draw(Magick::DrawableCompositeImage(star_x-star_size/2.0, star_y-star_size/2.0, star_size, star_size, avatar, Magick::OverCompositeOp));

            static std::uniform_int_distribution<unsigned int> openedDist(0, opened_gift_images.size()-1);
            static std::uniform_int_distribution<unsigned int> unopenedDist(0, unopened_gift_images.size()-1);
            for(unsigned int i=0; i<giftList.size(); i++) {
                const auto& gift = giftList[i];
                gift_image gi;
                if(gift.opened) {
                    if(opened_gift_images.empty()) continue;
                    gi = opened_gift_images[openedDist(engine)];
                } else {
                    if(unopened_gift_images.empty()) continue;
                    gi = unopened_gift_images[unopenedDist(engine)];
                }

                Magick::Image giftImage{};
                giftImage.read(i18n::resource_root+"/images"+std::string(gi.image));

                int y = image.rows() - giftImage.rows() - 200 + ((int) (i*(200.0/giftList.size())));
                int x;
                do
                {
                    x = std::uniform_int_distribution<int>(0, image.columns()-giftImage.columns())(engine);
                }
                while(false); // TODO: check for overlap

                image.draw(Magick::DrawableList{
                    Magick::DrawableTranslation(x, y),
                    Magick::DrawableCompositeImage(0, 0, giftImage.columns(), giftImage.rows(), giftImage, Magick::OverCompositeOp),
                    //Magick::DrawableFont("NotoSans-Regular"),
                    Magick::DrawablePointSize(gi.text_size),
                    Magick::DrawableTextAntialias(true),
                    Magick::DrawableFillColor(Magick::Color("black")),
                    Magick::DrawableTranslation(gi.text_x, gi.text_y),
                    Magick::DrawableRotation(gi.text_rotation),
                    Magick::DrawableText(0, 0, std::to_string(gift.id)),
                });

                auto sender_user_res = (co_await discord.co_user_get_cached(gift.sender));
                auto sender_member_res = (co_await discord.co_guild_get_member(guild.id, gift.sender));
                if(sender_user_res.is_error() || sender_member_res.is_error()) {
                    continue;
                }
                auto sender_user = sender_user_res.get<dpp::user_identified>();
                auto sender_member = sender_member_res.get<dpp::guild_member>();
                Magick::Image senderAvatar{};
                {
                    auto res = co_await discord.co_request(utils::get_effective_avatar_url(sender_member, sender_user), dpp::http_method::m_get);
                    const auto& img = res.body;
                    Magick::Blob inBlob{img.data(), img.size()};
                    senderAvatar.read(inBlob);
                }
                senderAvatar.composite(mask, 0, 0, Magick::CopyOpacityCompositeOp);
                image.draw(Magick::DrawableCompositeImage(
                    x+gi.avatar_x-gi.avatar_size/2.0,
                    y+gi.avatar_y-gi.avatar_size/2.0,
                    gi.avatar_size, gi.avatar_size, senderAvatar, Magick::OverCompositeOp));
            }

            {
                Magick::Blob outBlob{};
                image.write(&outBlob, "webp");

                event.reply(dpp::message{}
                    .add_file("image.webp", std::string(reinterpret_cast<const char*>(outBlob.data()), outBlob.length())));
            }
            co_return result::success;
        }

        dpp::coroutine<result> open_gift(pqxx::work& txn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, dpp::snowflake uid, int gift)
        {
            if(auto res = txn.exec_prepared(statements.get_gift, gift, uid, guild.id); !res.empty()) {
                auto [sender, amount, message, opened] = res.front().as<dpp::snowflake, int, std::optional<std::string>, bool>();
                if(opened) {
                    event.reply(utils::build_error(txn, guild, "command.christmas.error.opened"));
                    co_return result::user_error;
                }
                auto sender_user = (co_await discord.co_user_get_cached(sender)).get<dpp::user_identified>();

                db.change_coins(uid, guild.id, amount, txn);
                txn.exec_prepared0(statements.open_gift, gift, uid, guild.id);

                dpp::embed embed{};
                embed.set_title(i18n::translate(txn, guild, "command.christmas.title"));
                embed.set_color(branding::colors::cookie);
                embed.set_description(
                    i18n::translate(txn, guild, "command.christmas.message", sender_user.username, amount)
                    + message.transform([](const std::string& s) { return "\n\n" + s; }).value_or(""));
                embed.set_footer(sender_user.format_username(), sender_user.get_avatar_url());
                event.reply(dpp::message(event.msg.channel_id, embed));

                txn.commit();
            }
            else {
                event.reply(utils::build_error(txn, guild, "command.christmas.error.noent"));
                co_return result::user_error;
            }

            co_return result::success;
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& connection, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            pqxx::work txn{connection};

            auto now = std::chrono::system_clock::now();
            auto now_zoned = std::chrono::zoned_time{guild.timezone, now};
            std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(now_zoned.get_local_time())};
            if(ymd.month() != std::chrono::December)
            {
                co_return result::user_error;
            }
            if(static_cast<unsigned int>(ymd.day()) <= 23) {
                event.reply(utils::build_error(txn, guild, "command.christmas.error.early"));
                //co_return result::user_error;
            }

            dpp::snowflake uid = event.msg.author.id;
            int gift;
            if(args >> gift) {
                co_return co_await open_gift(txn, db, discord, guild, event, uid, gift);
            } else {
                co_return co_await show_gifts(txn, db, discord, guild, event, uid);
            }

            co_return result::success;
        }

        void prepare(chocobot&, database& db) override
        {
            statements.get_gift = db.prepare("get_gift", "SELECT sender, amount, message, opened FROM christmas_presents WHERE id=$1 AND uid=$2 AND guild=$3");
            statements.open_gift = db.prepare("open_gift", "UPDATE christmas_presents SET opened=true WHERE id=$1 AND uid=$2 AND guild=$3");
            statements.list_gifts = db.prepare("list_gifts", "SELECT id, sender, opened FROM christmas_presents WHERE uid=$1 AND guild=$2 AND (year = $3 OR opened=false)");
            statements.bot_gift = db.prepare("bot_gift", "INSERT INTO christmas_presents (uid, sender, guild, amount, message, year) VALUES($1, $2, $3, $4, $5, $6) RETURNING id");
        }
    private:
        static command_register<christmas_command> reg;
};
command_register<christmas_command> christmas_command::reg{};

}
