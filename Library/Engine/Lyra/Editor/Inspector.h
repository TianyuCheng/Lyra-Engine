#pragma once

#ifndef LYRA_LIBRARY_EDITOR_INSPECTOR_H
#define LYRA_LIBRARY_EDITOR_INSPECTOR_H

// local imports
#include "../Runtime/Application.h"

namespace lyra
{
    struct Inspector
    {
    public:
        explicit Inspector();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
    };
} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_INSPECTOR_H
