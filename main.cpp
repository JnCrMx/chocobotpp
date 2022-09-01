#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include "config.hpp"
#include "chocobot.hpp"

static int g_exiting = 0;
static chocobot::chocobot* g_bot;

int main(int argc, const char** argv)
{
    spdlog::cfg::load_env_levels();
    if(argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " [config file]" << std::endl;
        return 2;
    }

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
    g_bot = &bot;

    signal(SIGINT, [](int sig){
        if(g_exiting == 0)
        {
            g_exiting = 1;
            spdlog::info("Stopping ChocoBot...");
            g_bot->stop();
        }
        else if(g_exiting == 1)
        {
            g_exiting = 2;
            spdlog::warn("Already stopping. Press ^C again to force exit.");
        }
        else if(g_exiting == 2)
        {
            spdlog::critical("Quitting forcefully.");
            g_exiting = 3;
            _exit(128 + SIGINT);
        }
        else
        {
            spdlog::critical("Something went wrong after {} tries to exit.", g_exiting);
            std::terminate();
        }
    });
    bot.start();
    spdlog::info("Stopped ChocoBot");

    return 0;
}
