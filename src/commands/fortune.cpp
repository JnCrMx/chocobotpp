#include "paid_command.hpp"

#include "chocobot.hpp"
#include "i18n.hpp"

#include <fstream>
#include <filesystem>
#include <random>

namespace chocobot {

class fortune_command : public paid_command
{
    private:
        std::vector<std::string> fortune_messages{};
        std::uniform_int_distribution<int> dist;

        constexpr static int max_length = 4000;
    public:
        fortune_command() {}

        std::string get_name() override
        {
            return "glückskeks";
        }
        int get_cost() override
        {
            return 50;
        }

        dpp::coroutine<result> execute(chocobot&, pqxx::connection&, database&, dpp::cluster& discord, const guild&, const dpp::message_create_t& event, std::istream& args) override
        {
            static std::mt19937 rng(std::random_device{}());

            std::string message = fortune_messages.at(dist(rng));
            event.reply(message);

            co_return command::result::success;
        }
    private:
        static command_register<fortune_command> reg;

        void prepare(chocobot& bot, database&) override
        {
            std::string path = bot.cfg().fortunes_directory;
            if(path.empty())
            {
                using std::operator""s;
                const auto& candidates = {
                    "/usr/share/fortune/"s,
                    "/usr/share/games/fortunes/"s,
                    i18n::resource_root+"/fortunes/"
                };
                spdlog::debug("Trying to find fortunes in {}", fmt::join(candidates, ", "));
                auto p = std::find_if(candidates.begin(), candidates.end(), [](const std::string& p){
                    return std::filesystem::exists(p) && std::filesystem::is_directory(p);
                });

                if(p == candidates.end()) {
                    spdlog::error("Could not load find location of fortunes. Make sure that you have the appropiate package installed or specify the path in the config.");
                    return;
                }
                path = *p;
            }
            if(!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
            {
                spdlog::error("Cannot load fortunes from {}. It either does not exist or is not a directory.", path);
                return;
            }
            spdlog::debug("Loading fortune files from directory {}", path);

            int files = 0, too_long_total = 0;
            for(const auto& ent : std::filesystem::recursive_directory_iterator(path))
            {
                if(!ent.is_regular_file()) continue;
                if(ent.path().filename().string().ends_with(".dat")) continue;

                spdlog::trace("Loading fortunes from file {}", ent.path().string());

                std::ifstream in(ent.path());
                std::string fortune{};
                std::string line;

                int count = 0, lines = 0, too_long = 0;
                while(std::getline(in, line, '\n'))
                {
                    if(line == "%") {
                        if(fortune.size() <= (max_length+1))
                        {
                            fortune_messages.push_back(fortune.substr(0, fortune.size()-1));
                            count++;
                        }
                        else { too_long++; }
                        fortune = {};
                    } else {
                        fortune += line + "\n";
                    }
                    lines++;
                }
                spdlog::trace("Loaded {} ({} were too long to load) fortunes from {} lines from file {}", count, too_long, lines, ent.path().string());
                files++;
                too_long_total += too_long;
            }
            spdlog::info("Loaded {} from fortunes from {} files in directory {}", fortune_messages.size(), files, path);
            if(too_long_total != 0) {
                spdlog::warn("{} ({:.1f}%) fortunes were too long (over {} character) and were therefore ignored.", too_long_total,
                    static_cast<double>(too_long_total)/static_cast<double>(too_long_total + fortune_messages.size()), max_length);
            }
            dist = std::uniform_int_distribution<>(0, fortune_messages.size()-1);
        }
};
command_register<fortune_command> fortune_command::reg{};

}
