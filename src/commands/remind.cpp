#include "command.hpp"
#include "utils.hpp"
#include "i18n.hpp"

#include <date/date.h>
#include <date/tz.h>

namespace chocobot {

class remind_command : public command
{
    private:
		constexpr static std::size_t max_message_length = 1000;

        std::string statement;
    public:
        remind_command() {}

        std::string get_name() override
        {
            return "remind";
        }

	private:
		static bool try_parse(const std::string& str, const std::string& format, date::local_time<std::chrono::milliseconds>& time)
		{
			std::istringstream iss(str);
			iss >> date::parse(format, time);
			return time != date::local_time<std::chrono::milliseconds>{};
		}

		static date::local_time<std::chrono::milliseconds> parse_time(const std::string& str, date::local_time<std::chrono::milliseconds> now)
		{
			date::local_time<std::chrono::milliseconds> time{};
			std::chrono::years year = std::chrono::duration_cast<std::chrono::years>(now.time_since_epoch());
			std::chrono::days today = std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch());

			if(try_parse(str, "%d.%m.%Y/%H:%M", time)) return time;
			// if(try_parse("1970###"+str, "%Y###%d.%m/%H:%M", time)) return time + year; // we don't support this for now
			if(try_parse("1970-01-01###"+str, "%F###%H:%M", time))
			{
				auto t = time + today;
				if(t < now)
					t += std::chrono::days(1);
				return t;
			}

			// TODO: parse durations

			spdlog::warn("Cannot parse date: {}", str);
			throw std::runtime_error("cannot parse date: "+str);
		}

	public:
		result execute(chocobot&, pqxx::connection& connection, database&, dpp::cluster& discord, const guild& guild, const dpp::message_create_t& event, std::istream& args) override
        {
			std::string first;
			args >> first;

			auto snowflake = utils::parse_mention(first);
			dpp::snowflake user = snowflake.value_or(event.msg.author.id);

			dpp::user other_user;
			if(user != event.msg.author.id)
			{
				try
				{
					other_user = discord.user_get_cached_sync(user);
				}
				catch(const dpp::rest_exception& ex)
				{
					event.reply(utils::build_error(connection, guild, "command.remind.error.noent"));
					return command::result::user_error;
				}
			}

			date::local_time<std::chrono::milliseconds> time;
			date::local_time<std::chrono::milliseconds> now = date::make_zoned(guild.timezone, date::floor<std::chrono::milliseconds>(std::chrono::system_clock::now())).get_local_time();

			try
			{
				if(snowflake.has_value())
				{
					std::string d;
					args >> d;
					time = parse_time(d, now);
				}
				else
				{
					time = parse_time(first, now);
				}
			}
			catch(const std::runtime_error& ex)
			{
				event.reply(utils::build_error(connection, guild, "command.remind.error.fmt"));
				return command::result::user_error;
			}
			while(args.peek() == ' ') args.get(); // skip trailing whitespace of message

			auto zoned = date::make_zoned(guild.timezone, time);

			std::string message(std::istreambuf_iterator<char>(args), {});
			//spdlog::debug("{} wants to remind {} at {} with message \"{}\"", event.msg.author.id, user, zoned, message);

			if(message.length() > max_message_length) {
				event.reply(utils::build_error(connection, guild, "command.remind.error.length"));
				return command::result::user_error;
			}

			if(time < now)
			{
				event.reply(utils::build_error(connection, guild, "command.remind.error.past"));
				return command::result::user_error;
			}

			{
				pqxx::work txn(connection);
				txn.exec_prepared0(statement, user, guild.id,
					message.empty()?std::optional<std::string>{}:message,
					std::chrono::duration_cast<std::chrono::milliseconds>(zoned.get_sys_time().time_since_epoch()).count(),
					event.msg.author.id, event.msg.channel_id);
				txn.commit();
			}

            auto forced_sys = std::chrono::sys_time<std::chrono::milliseconds>(time.time_since_epoch());
			if(user == event.msg.author.id)
			{
				event.reply(dpp::message{dpp::snowflake{}, i18n::translate(connection, guild, "command.remind.self", forced_sys)});
			}
			else
			{
				event.reply(dpp::message{dpp::snowflake{}, i18n::translate(connection, guild, "command.remind.other", forced_sys, other_user.format_username())});
			}

			return result::success;
		}

		void prepare(chocobot&, database& db) override
        {
            statement = db.prepare("insert_remind", "INSERT INTO reminders (uid, guild, message, time, issuer, channel) VALUES($1, $2, $3, $4, $5, $6)");
        }
    private:
        static command_register<remind_command> reg;
};
command_register<remind_command> remind_command::reg{};

}
