#pragma once

#ifndef LYRA_LYRA_EDITOR_LOGGER_VIEW_H
#define LYRA_LYRA_EDITOR_LOGGER_VIEW_H

// local imports
#include <Lyra/Player/Application.h>

namespace lyra
{
    struct LoggerView
    {
    public:
        explicit LoggerView(size_t capacity);

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        void show_bar();
        void show_logs() const;

    private:
        uint8_t level_filter = 0x3F; // Bitmask for individual level toggles
        char    filter[1024];
        bool    auto_scroll  = true;
    };
} // namespace lyra

#endif // LYRA_LYRA_EDITOR_LOGGER_VIEW_H
