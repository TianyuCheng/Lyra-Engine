#pragma once

#ifndef LYRA_LIBRARY_EDITOR_TREE_VIEW_H
#define LYRA_LIBRARY_EDITOR_TREE_VIEW_H

// local imports
#include "../Runtime/Application.h"

namespace lyra
{
    struct TreeView
    {
    public:
        explicit TreeView();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
    };
} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_TREE_VIEW_H
