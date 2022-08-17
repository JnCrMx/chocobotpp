#pragma once

#include <dpp/dpp.h>
#include <pqxx/pqxx>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <memory>

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
            system_error
        };
        template<typename OStream>
        friend OStream &operator<<(OStream& os, const result& r)
        {
            switch(r)
            {
                case command::result::success:      return os << "success";
                case command::result::user_error:   return os << "user_error";
                case command::result::system_error: return os << "system_error";
            }
        }
        static spdlog::level::level_enum result_level(const result& r)
        {
            switch(r)
            {
                case result::success:
                case result::user_error:
                    return spdlog::level::info;
                case result::system_error:
                    return spdlog::level::warn;
            }
        }

        virtual result execute(chocobot&, pqxx::connection&, database&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&) = 0;
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
};

};
