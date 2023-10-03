#pragma once

#include <fmt/format.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <memory>
#include <dpp/cluster.h>
#include <dpp/coro/coroutine.h>

#include "database.hpp"

namespace chocobot {

class chocobot;

class command
{
    public:
        enum class result
        {
            success,
            user_error,
            system_error,
            deferred
        };

        template<typename OStream>
        friend OStream &operator<<(OStream& os, const result& r)
        {
            switch(r)
            {
                case command::result::success:      return os << "success";
                case command::result::user_error:   return os << "user_error";
                case command::result::system_error: return os << "system_error";
                case command::result::deferred:     return os << "deferred";
            }
        }
        static spdlog::level::level_enum result_level(const result& r)
        {
            switch(r)
            {
                case result::success:
                case result::user_error:
                case result::deferred:
                    return spdlog::level::info;
                case result::system_error:
                    return spdlog::level::warn;
            }
            return spdlog::level::warn;
        }

        virtual dpp::coroutine<result> execute(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&) = 0;
        virtual std::string get_name() = 0;
        virtual void prepare(chocobot&, database&) {}
        virtual ~command() {}
};

struct command_factory
{
    using map_type = std::map<std::string, std::unique_ptr<command>>;

	public:
   		static map_type * get_map()
        {
       		if(map == nullptr)
            {
                map = new map_type;
            }
        	return map;
   		}

	private:
   		static map_type * map;
};

template<typename T>
struct command_register : command_factory
{
    command_register(T* command)
    {
        get_map()->insert(std::make_pair(command->get_name(), std::unique_ptr<class command>(command)));
        spdlog::info("Registered command {}", command->get_name());
    }

    command_register() : command_register(new T) {}

    template<typename... Args>
    command_register(Args&&... args) : command_register(new T(std::forward(args...))) {}
};

};

template<> struct fmt::formatter<chocobot::command::result> : formatter<std::string_view>
{
    template<typename FormatContext>
    auto format(chocobot::command::result r, FormatContext& ctx) const
    {
        std::string_view s = "unknown";
        switch(r)
        {
            case chocobot::command::result::success:      s = "success"; break;
            case chocobot::command::result::user_error:   s = "user_error"; break;
            case chocobot::command::result::system_error: s = "system_error"; break;
            case chocobot::command::result::deferred:     s = "deferred"; break;
        }
        return formatter<string_view>::format(s, ctx);
    }
};
