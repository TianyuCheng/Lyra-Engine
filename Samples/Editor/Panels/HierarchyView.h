#pragma once

#ifndef LYRA_EDITOR_PANELS_HIERARCHY_VIEW_H
#define LYRA_EDITOR_PANELS_HIERARCHY_VIEW_H

#include <Lyra/Scenes/SceneTree.h>
#include <Lyra/Scenes/SceneNode.h>

// local imports
#include <Lyra/Engine/Application.h>

namespace lyra
{
    struct HierarchyView
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

        explicit HierarchyView();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        SceneTree::NodeIndex selected_node      = SceneTree::INVALID_NODE;
        char                 search_filter[256] = "";
    };
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_HIERARCHY_VIEW_H
