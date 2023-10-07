#include "paid_command.hpp"

#include "chocobot.hpp"
#include "i18n.hpp"

#include <Magick++.h>
#include <fmt/ranges.h>

namespace chocobot {

class magick_command : public paid_command
{
    private:
        using magick_operation = std::function<void(Magick::Image&, std::istringstream&, const dpp::message_create_t& event)>;
        static std::unordered_map<std::string, magick_operation> operations;

        static constexpr std::size_t max_size = 1024*1024;
    public:
        magick_command() {}

        std::string get_name() override
        {
            return "magick";
        }
        int get_cost() override
        {
            return 100;
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection& conn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            bool use_attachment = !event.msg.attachments.empty() && event.msg.attachments.front().size <= max_size;
            std::string url = use_attachment ? event.msg.attachments.front().url: event.msg.author.get_avatar_url();
            auto res = co_await discord.co_request(url, dpp::http_method::m_get);
            const auto& img = res.body;
            Magick::Blob blob{img.data(), img.size()};
            Magick::Image image{};
            image.read(blob);

            std::string operation;
            while(args >> std::quoted(operation))
            {
                if(operation.ends_with(")"))
                    operation.erase(operation.size()-1);
                std::istringstream iss(operation);
                std::string keyword;
                std::getline(iss, keyword, '(');

                if(!operations.contains(keyword)) {
                    pqxx::nontransaction txn{conn};
                    dpp::embed embed{};
                    embed.set_title(i18n::translate(txn, guild, "error"));
                    embed.set_color(branding::colors::error);
                    embed.set_description(i18n::translate(txn, guild, "command.magick.error.unknown_operation", keyword));
                    event.reply(dpp::message({}, embed));
                    continue;
                }
                operations[keyword](image, iss, event);
            }

            image.write(&blob, "png");

            event.reply(dpp::message{}
                .add_file("image.png", std::string(reinterpret_cast<const char*>(blob.data()), blob.length())));
            co_return result::success;
        }
    private:
        static command_register<magick_command> reg;
};
command_register<magick_command> magick_command::reg{};

void read_args_and_call(auto func, std::istringstream& iss, auto... defaults)
{
    auto read_one = [&](auto& x){
        std::remove_cvref_t<decltype(x)> v = x;
        bool ok = false;
        if constexpr (std::is_same_v<decltype(v), std::string>) {
            ok = static_cast<bool>(iss >> std::quoted(v));
        } else {
            ok = static_cast<bool>(iss >> v);
        }
        if(ok) {
            x = v;
        }
    };
    (read_one(defaults), ...);
    spdlog::trace("Calling Magick function with {}", std::make_tuple(defaults...));
    func(defaults...);
}
#define BASIC_OP(name) {#name, [](Magick::Image& image, auto&, auto&){image.name();}}
#define ARG_OP(name, ...) {#name, [](Magick::Image& image, auto& iss, auto&){\
    read_args_and_call(std::bind_front(&Magick::Image::name, &image), iss, __VA_ARGS__); \
}}

std::unordered_map<std::string, magick_command::magick_operation> magick_command::operations = {
    {"help", [](auto&, auto&, const dpp::message_create_t& event){
        std::string message = "## Operations:";
        for(const auto& [k, _] : magick_command::operations)
        {
            message += "\n- " + k;
        }
        event.reply(message);
    }},
#if MagickLibInterface == 6
    ARG_OP(oilPaint, 3.0),
    ARG_OP(polaroid, std::string{}, 0.0),
    ARG_OP(posterize, 16, false),
#elif MagickLibInterface == 6
    ARG_OP(oilPaint, 0.0, 1.0),
    ARG_OP(polaroid, std::string{}, 0.0, PixelInterpolateMethod{}),
    ARG_OP(posterize, 16, DitherMethod{}),
#endif
    ARG_OP(negate, false),
    BASIC_OP(normalize),
    BASIC_OP(enhance),
    BASIC_OP(magnify),
    BASIC_OP(minify),
    BASIC_OP(equalize),
    BASIC_OP(flip),
    BASIC_OP(flop),
    BASIC_OP(quantize),
    BASIC_OP(raise),
    BASIC_OP(reduceNoise),
    ARG_OP(shade, 30.0, 30.0, false),
    ARG_OP(shadow, 80.0, 0.5, 5, 5),
    ARG_OP(sharpen, 0.0, 1.0),
    ARG_OP(solarize, 50.0),
    ARG_OP(spread, 3ul),
    BASIC_OP(transpose),
    BASIC_OP(transverse),
    ARG_OP(vignette, 0.0, 1.0, 0, 0),
    ARG_OP(wave, 25.0, 150.0),
    ARG_OP(adaptiveBlur, 0.0, 1.0),
    ARG_OP(adaptiveSharpen, 0.0, 1.0),
    BASIC_OP(autoGamma),
    BASIC_OP(autoLevel),
    BASIC_OP(autoOrient),
    ARG_OP(blueShift, 1.5),
    ARG_OP(rotate, 15),
    ARG_OP(rotationalBlur, 15),
    ARG_OP(sepiaTone, 0.8),
    ARG_OP(sketch, 0.0, 1.0, 0.0),
};

}
