#pragma once

#ifndef LYRA_LIBRARY_EDITOR_HIERARCHY_H
#define LYRA_LIBRARY_EDITOR_HIERARCHY_H

// local imports
#include "../Runtime/Application.h"

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
