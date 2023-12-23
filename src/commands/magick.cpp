#include "paid_command.hpp"

#include "chocobot.hpp"
#include "i18n.hpp"
#include "utils.hpp"

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
            bool use_attachment = !event.msg.attachments.empty();
            if(use_attachment) {
                auto size = event.msg.attachments.front().size;
                if(size > max_size) {
                    pqxx::nontransaction txn{conn};
                    dpp::embed embed{};
                    embed.set_title(i18n::translate(txn, guild, "error"));
                    embed.set_color(branding::colors::error);
                    embed.set_description(i18n::translate(txn, guild, "command.magick.error.size", size, max_size));
                    event.reply(dpp::message({}, embed));
                    co_return result::user_error;
                }
            }
            std::string url = use_attachment ? event.msg.attachments.front().url : utils::get_effective_avatar_url(event.msg.member, event.msg.author);

            Magick::Image image{};

            try
            {
                auto res = co_await discord.co_request(url, dpp::http_method::m_get);
                const auto& img = res.body;
                Magick::Blob inBlob{img.data(), img.size()};
                image.read(inBlob);
            }
            catch(const std::exception& ex) {
                pqxx::nontransaction txn{conn};
                dpp::embed embed{};
                embed.set_title(i18n::translate(txn, guild, "error"));
                embed.set_color(branding::colors::error);
                embed.set_description(i18n::translate(txn, guild, "command.magick.error.read", url, ex.what()));
                event.reply(dpp::message({}, embed));
                co_return result::system_error;
            }

            std::string operation;
            while(args >> std::quoted(operation))
            {
                if(operation == "help") {
                    pqxx::nontransaction txn{conn};
                    std::string message = i18n::translate(txn, guild, "command.magick.subhelp.title");
                    for(const auto& [k, _] : magick_command::operations)
                    {
                        message += '\n' + i18n::translate(txn, guild, "command.magick.subhelp.line", k);
                    }
                    event.reply(message);
                    co_return result::user_error; // prevent costing coins in this case
                }

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
                    co_return result::user_error;
                }
                try
                {
                    operations[keyword](image, iss, event);
                }
                catch(const std::exception& ex)
                {
                    pqxx::nontransaction txn{conn};
                    dpp::embed embed{};
                    embed.set_title(i18n::translate(txn, guild, "error"));
                    embed.set_color(branding::colors::error);
                    embed.set_description(i18n::translate(txn, guild, "command.magick.error.operation_failed", operation, ex.what()));
                    event.reply(dpp::message({}, embed));
                    co_return result::system_error;
                }
            }

            {
                Magick::Blob outBlob{};
                image.write(&outBlob, "webp");

                event.reply(dpp::message{}
                    .add_file("image.webp", std::string(reinterpret_cast<const char*>(outBlob.data()), outBlob.length())));
            }
            co_return result::success;
        }
    private:
        static command_register<magick_command> reg;
};
command_register<magick_command> magick_command::reg{};

void read_args_and_call(auto func, std::istringstream& iss, auto... defaults)
{
    auto read_one = [&](auto& x){
        using type = std::remove_cvref_t<decltype(x)>;
        type v = x;
        bool ok = false;
        if constexpr (std::is_same_v<type, std::string>) {
            ok = static_cast<bool>(iss >> std::quoted(v));
        }
        else if constexpr(std::is_enum_v<type>){
            std::underlying_type_t<type> vv;
            ok = static_cast<bool>(iss >> vv);
            v = static_cast<type>(vv);
        }
        else {
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
#if MagickLibInterface == 6
    ARG_OP(oilPaint, 3.0),
    ARG_OP(polaroid, std::string{}, 0.0),
    ARG_OP(posterize, 16, false),
#elif MagickLibInterface == 10
    ARG_OP(oilPaint, 0.0, 1.0),
    ARG_OP(polaroid, std::string{}, 0.0, Magick::NearestInterpolatePixel),
    ARG_OP(posterize, 16, Magick::NoDitherMethod),
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
