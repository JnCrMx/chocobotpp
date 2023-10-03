#include "command.hpp"
#include "utils.hpp"
#include "chocobot.hpp"
#include "i18n.hpp"
#include "branding.hpp"

#ifdef __unix__
#include <ext/stdio_filebuf.h>
#include <sys/wait.h>
#endif

namespace chocobot {

class bugreport_command : public command
{
    public:
        bugreport_command() {}

        std::string get_name() override
        {
            return "bugreport";
        }

        dpp::coroutine<result> execute(chocobot& bot, pqxx::connection& conn, database& db,
            dpp::cluster& discord, const guild& guild,
            const dpp::message_create_t& event, std::istream& args) override
        {
            // skip space
            args.get();
            if(!args.good())
            {
                event.reply(utils::build_error(conn, guild, "command.bugreport.error.empty"));
                co_return result::user_error;
            }

            std::string subject;
            std::getline(args, subject);

            if(subject.empty())
            {
                event.reply(utils::build_error(conn, guild, "command.bugreport.error.empty"));
                co_return result::user_error;
            }

            std::string body{std::istreambuf_iterator<char>(args), {}};

#ifdef __unix__
            struct sigaction sa_new{}, sa_old{};
            sa_new.sa_handler = SIG_DFL;
            sigemptyset(&sa_new.sa_mask);
            sigaction(SIGCHLD, &sa_new, &sa_old);

            int fd[2];
            pipe(fd);
            pid_t childpid = fork();
            if(childpid == 0)
            {
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                close(fd[1]);

                execlp("/bin/sh", "sh", "-c", bot.cfg().bugreport_command.c_str(), (char*) NULL);
                _exit(127);
            }
            else
            {
                std::thread thread([fd, subject, body, childpid, sa_old, event, &db, guild](){
                    close(fd[0]);

                    __gnu_cxx::stdio_filebuf<char> fbuf(fd[1], std::ios_base::out);
                    std::ostream fs(&fbuf);
                    fs << "Subject: [ChocoBot Bugreport] " << subject << "\n\n";
                    if(!body.empty())
                        fs << body << "\n\n";
                    fs << "Sent by " << event.msg.author.format_username() << " (" << event.msg.author.id << ")"
                       << " with message id " << event.msg.id
                       << " in channel " << event.msg.channel_id << " in guild " << event.msg.guild_id
                       << "." << std::endl;
                    close(fd[1]);

                    int status{};
                    pid_t ret = waitpid(childpid, &status, 0);

                    sigaction(SIGCHLD, &sa_old, NULL);

                    auto conn = db.acquire_connection();
                    if(ret != -1 && WEXITSTATUS(status) != 0)
                    {
                        event.reply(utils::build_error(*conn, guild, "command.bugreport.error.general"));
                    }
                    else
                    {
                        dpp::embed eb{};
                        eb.set_title(i18n::translate(*conn, guild, "command.bugreport.title"));
                        eb.set_color(branding::colors::cookie);
                        eb.set_description(i18n::translate(*conn, guild, "command.bugreport.message"));
                        event.reply(dpp::message(dpp::snowflake{}, eb));
                    }
                });
                thread.detach();
                co_return result::deferred;
            }
#else
            event.reply(utils::build_error(conn, guild, "command.bugreport.error.unsupported"));
            return result::system_error;
#endif
        }
    private:
        static command_register<bugreport_command> reg;
};
command_register<bugreport_command> bugreport_command::reg{};

}
