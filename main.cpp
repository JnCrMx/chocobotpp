#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include "config.hpp"
#include "chocobot.hpp"
#include "utils.hpp"

static int g_exiting = 0;
static chocobot::chocobot* g_bot;

constexpr std::array CONFIG_CANDIDATES = {"./config.json", "/etc/chocobot.json", "/etc/chocobot/config.json"};
constexpr auto ENV_CONFIG = "CHOCOBOT_CONFIG_FILE";
constexpr auto ENV_ROOT = "CHOCOBOT_ROOT";

static void print_docs(std::string arg0)
{
    std::cerr << "Usage: " << arg0 << " [config file]" << std::endl;
    std::cerr << R"EOF(
If no argument is specified ChocoBot searched for the config file in the following locations:
1. $CHOCOBOT_CONFIG_FILE
2. $CHOCOBOT_ROOT/config.json
3. ./config.json
4. /etc/chocobot.json
5. /etc/chocobot/config.json

ChocoBot always searches for resources in the following locations:
1. $CHOCOBOT_RESOURCES_PATH
2. $CHOCOBOT_ROOT/resources
3. ./resources
4. /usr/local/share/chocobot/resources
5. /usr/share/chocobot/resources
)EOF";
}

int main(int argc, const char** argv)
{
    spdlog::cfg::load_env_levels();
    if(argc > 2)
    {
        print_docs(argv[0]);
        return 2;
    }
    std::string configFile;
    if(argc == 2)
    {
        configFile = argv[1];
    }
    else
    {
        std::vector<std::optional<std::string>> paths = {chocobot::utils::get_env(ENV_CONFIG), chocobot::utils::get_env(ENV_ROOT).transform([](std::string s){return s+"/config.json";})};
        paths.insert(paths.end(), CONFIG_CANDIDATES.begin(), CONFIG_CANDIDATES.end());
        try
        {
            configFile = chocobot::utils::find_exists(paths);
        }
        catch(const std::runtime_error&)
        {
            spdlog::error("Failed to locate config");
            for(const auto& p : paths)
            {
                if(p)
                {
                    spdlog::debug("Tried path: {}", *p);
                }
            }
            print_docs(argv[0]);
            return 2;
        }
    }

    spdlog::info("Starting ChocoBot...");
    spdlog::debug("Using config {}", configFile);

    chocobot::config config{};
    {
        nlohmann::json j;
        {
            std::ifstream ifconfig(configFile);
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
