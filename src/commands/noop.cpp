#include "command.hpp"

namespace chocobot {

class noop_command : public command
{
    public:
        noop_command() {}

        std::string get_name() override
        {
            return "noop";
        }

        result execute(chocobot&, pqxx::connection&, database&, 
            dpp::cluster&, const guild&,
            const dpp::message_create_t&, std::istream&) override
        {
            return result::success;
        }
    private:
        static command_register<noop_command> reg;
};
command_register<noop_command> noop_command::reg{};

}
