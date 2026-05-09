#pragma once

#ifndef LYRA_LYRA_COMMON_LOGGER_H
#define LYRA_LYRA_COMMON_LOGGER_H

// vendor headers
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// library headers
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Common/Compatibility.h> // Now includes get_environment_variable

namespace lyra
{
    using Logger   = Ref<spdlog::logger>;
    using LogView  = fmt::string_view;
    using LogLevel = spdlog::level::level_enum;

    inline LogLevel parse_log_level(const std::string& level)
    {
        if (level == "trace")
            return LogLevel::trace;
        if (level == "debug")
            return LogLevel::debug;
        if (level == "info")
            return LogLevel::info;
        if (level == "warn")
            return LogLevel::warn;
        if (level == "error")
            return LogLevel::err;
        if (level == "critical")
            return LogLevel::critical;
        if (level == "off")
            return LogLevel::off;
        return LogLevel::info; // default to info if no match
    }

    inline LogLevel parse_log_level_from_env(CString env)
    {
        auto env_value = get_environment_variable(env);
        if (env_value) {
            return parse_log_level(env_value.value().c_str());
        }
        return LogLevel::info; // Default to info if environment variable is not set or empty
    }

    struct ConsoleLog
    {
        LogLevel verbosity = LogLevel::info;
        LogView  component = "";
        String   payload   = "";
    };

    struct ConsoleSink
    {
    public:
        explicit ConsoleSink(size_t capacity = 2048);

        void clear();
        void resize(size_t new_capacity);
        void append(LogLevel level, LogView component, String&& payload);

        bool modified() const { return changed; }

        // only clear "changed" flag
        void reset() { changed = false; }

        size_t count(spdlog::level_t level) const;

        template <typename F>
        void for_each(F&& f) const
        {
            for (auto& log : logs)
                f(log);
        }

    private:
        using Logs   = RingBuffer<ConsoleLog>;
        using Counts = Array<size_t, spdlog::level::n_levels>;

        Logs   logs;
        Counts counts;
        bool   changed = false;
    };

    template <typename Mutex>
    struct console_sink : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        explicit console_sink() : console(2048)
        {
            // do nothing
        }

        auto get_console() -> ConsoleSink& { return console; }

        auto get_console() const -> const ConsoleSink& { return console; }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            // format message
            spdlog::memory_buf_t formatted;
            this->formatter_->format(msg, formatted);

            // forward to your editor’s console system
            auto text = fmt::to_string(formatted);
            console.append(msg.level, msg.logger_name, std::move(text));
        }

        void flush_() override
        {
            // no-op or flush editor console buffer
        }

    private:
        ConsoleSink console;
    };

    using console_sink_mt = console_sink<std::mutex>;
    using console_sink_st = console_sink<spdlog::details::null_mutex>;

    auto get_stdout_sink() -> Ref<spdlog::sinks::stdout_color_sink_mt>;
    auto get_stderr_sink() -> Ref<spdlog::sinks::stderr_color_sink_mt>;
    auto get_console_sink() -> Ref<console_sink_mt>;
    auto create_logger(const String& name, LogLevel level) -> Logger;
    auto create_default_logger() -> Logger;

} // end of namespace lyra

#endif // LYRA_LYRA_COMMON_LOGGER_H
