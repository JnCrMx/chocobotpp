module;

#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

export module dpp;

export namespace dpp {
    using dpp::snowflake;
    using dpp::cluster;
    using dpp::message;
    using dpp::message_map;
    using dpp::user;
    using dpp::user_identified;
    using dpp::guild;
    using dpp::guild_member;
    using dpp::guild_map;
    using dpp::guild_member_map;
    using dpp::role;
    using dpp::role_map;
    using dpp::channel;

    using dpp::embed;
    using dpp::embed_author;
    using dpp::embed_footer;

    using dpp::image_type;

    using dpp::find_user;

    using dpp::event_handle;
    using dpp::log_t;
    using dpp::message_create_t;
    using dpp::message_reaction_add_t;
    using dpp::message_reaction_remove_t;
    using dpp::http_request_completion_t;

    using dpp::http_method;
    using dpp::http_error;
    using dpp::loglevel;
    using dpp::rest_exception;
    using dpp::confirmation;

    using dpp::coroutine;
    using dpp::job;
    using dpp::when_any;

    namespace utility {
        using dpp::utility::cdn_endpoint_url_hash;
    }
}

export namespace nlohmann {
    using nlohmann::json;
    using nlohmann::to_string;

    namespace detail {
        using nlohmann::detail::iteration_proxy;
        using nlohmann::detail::get;
    }
}
