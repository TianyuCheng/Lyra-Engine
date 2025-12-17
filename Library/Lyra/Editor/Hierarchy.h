#pragma once

#ifndef LYRA_LIBRARY_EDITOR_HIERARCHY_H
#define LYRA_LIBRARY_EDITOR_HIERARCHY_H

#include <Lyra/Engine/Applet/Application.h>

namespace lyra
{
    struct Hierarchy
    {
    public:
        explicit Hierarchy();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
    };
} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_HIERARCHY_H
