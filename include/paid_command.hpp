#pragma once

#include "command.hpp"

namespace chocobot {

class paid_command : public command {
    public:
        virtual dpp::coroutine<bool> preflight(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&) override;
        virtual dpp::coroutine<> postexecute(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&, result) override;
        virtual int get_cost() = 0;
};

}
