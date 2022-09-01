#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include "config.hpp"
#include "chocobot.hpp"

int main(int argc, const char** argv)
{
    spdlog::cfg::load_env_levels();
    if(argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " [config file]" << std::endl;
        return 2;
    }

    signal(SIGINT, [](int sig){std::exit(0);});

    spdlog::info("Starting ChocoBot...");

    chocobot::config config{};
    {
        nlohmann::json j;
        {
            std::ifstream ifconfig(argv[1]);
            ifconfig >> j;
        }
        config = {j};
    }
    chocobot::chocobot bot(config);
    bot.start();

    return 0;
}
