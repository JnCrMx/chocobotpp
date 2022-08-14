#pragma once

#include <dpp/dpp.h>
#include <spdlog/spdlog.h>
#include <memory>

#include "database.hpp"

namespace chocobot {

class chocobot;

class command
{
    public:
        virtual bool execute(chocobot&, dpp::cluster&, const guild&, const dpp::message_create_t&, std::istream&) = 0;
        virtual std::string get_name() = 0;
        virtual void prepare() {}
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
