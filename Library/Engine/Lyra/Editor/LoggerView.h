#pragma once

#ifndef LYRA_LIBRARY_EDITOR_LOG_VIEW_H
#define LYRA_LIBRARY_EDITOR_LOG_VIEW_H

// local imports
#include "../Runtime/Application.h"

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
        int  log_level = 0;
        char filter[1024];
    };
} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_LOG_VIEW_H
