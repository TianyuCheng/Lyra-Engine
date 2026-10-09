#pragma once

#ifndef LYRA_EDITOR_PANELS_HIERARCHY_VIEW_H
#define LYRA_EDITOR_PANELS_HIERARCHY_VIEW_H

#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/SceneNode.h>

// local imports
#include <Lyra/Runtime/Application.h>

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

        void update(AppContext& context);

    private:
        SceneTree::NodeIndex selected_node             = SceneTree::INVALID_NODE;
        char                 search_filter[256]        = "";
        bool                 open_rename_modal         = false;
        bool                 show_rename_modal         = false;
        Entity               entity_to_rename          = entt::null;
        char                 rename_buffer[256]        = "";
        Entity               entity_to_duplicate       = entt::null;
        Entity               entity_to_delete_children = entt::null;
    };
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_HIERARCHY_VIEW_H
