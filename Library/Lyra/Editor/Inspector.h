#pragma once

#ifndef LYRA_LIBRARY_ENGINE_EDITOR_INSPECTOR_H
#define LYRA_LIBRARY_ENGINE_EDITOR_INSPECTOR_H

#include <Lyra/Engine/Applet/Application.h>

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

#endif // LYRA_LIBRARY_ENGINE_EDITOR_INSPECTOR_H
