module;

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <spdlog/cfg/argv.h>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

export module spdlog;

export namespace spdlog {
    using spdlog::log;
    using spdlog::trace;
    using spdlog::debug;
    using spdlog::info;
    using spdlog::warn;
    using spdlog::error;
    using spdlog::critical;
    using spdlog::set_level;
    using spdlog::should_log;

    namespace level {
        using spdlog::level::trace;
        using spdlog::level::debug;
        using spdlog::level::info;
        using spdlog::level::warn;
        using spdlog::level::err;
        using spdlog::level::critical;

        using spdlog::level::level_enum;
    }
    namespace cfg {
        using spdlog::cfg::load_env_levels;
        using spdlog::cfg::load_argv_levels;
    }
};

export namespace fmt {
    using fmt::formatter;
    using fmt::string_view;
    using fmt::arg;
    using fmt::join;
    using fmt::format;
}
