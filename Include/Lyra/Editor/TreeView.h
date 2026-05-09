#pragma once

#ifndef LYRA_LYRA_EDITOR_TREE_VIEW_H
#define LYRA_LYRA_EDITOR_TREE_VIEW_H

#include <Lyra/Scenes/SceneTree.h>
#include <Lyra/Scenes/SceneNode.h>

// local imports
#include <Lyra/Player/Application.h>

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

#endif // LYRA_LYRA_EDITOR_TREE_VIEW_H
