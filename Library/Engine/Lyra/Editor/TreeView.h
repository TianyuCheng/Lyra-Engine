#pragma once

#ifndef LYRA_LIBRARY_EDITOR_TREE_VIEW_H
#define LYRA_LIBRARY_EDITOR_TREE_VIEW_H

#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/SceneNode.h>

// local imports
#include "../Runtime/Application.h"

namespace lyra
{
    struct TreeView
    {
    public:
        struct Expansion
        {
            bool expanded = false;
        };

        struct Selection
        {
            SceneNode node;
        };

        explicit TreeView();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        SceneTree::NodeIndex selected_node = SceneTree::INVALID_NODE;
    };
} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_TREE_VIEW_H
