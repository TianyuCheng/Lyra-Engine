#include <string>
#include <algorithm>

#include <Lyra/Common/Logger.h>
#include <Lyra/Scenes/Camera.h>
#include <Lyra/Scenes/SceneTree.h>
#include <Lyra/Scenes/SceneNode.h>
#include <Lyra/UICore/UI.h>
#include <Lyra/UICore/UITree.h>
#include <Lyra/UICore/UIDock.h>
#include <Lyra/UICore/UILayout.h>
#include <Lyra/UICore/UIControls.h>

// local imports
#include <Lyra/UICore/UIIcons.h>
#include "Common/EditorLayout.h"
#include "HierarchyView.h"

#define LYRA_TREE_VIEW_WINDOW_NAME (LYRA_ICON_TREE " Hierarchy")

using namespace lyra;

HierarchyView::HierarchyView()
{
    // do nothing
}

void HierarchyView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &HierarchyView::update>(*this);
}

static void render_node(World& world, SceneTree& hierarchy, SceneTree::NodeIndex node_idx, SceneTree::NodeIndex& selected_node, Blackboard& blackboard, const char* filter)
{
    const auto& node   = hierarchy.at(node_idx);
    const auto  entity = node.entity;

    bool is_leaf = (node.first_child == SceneTree::INVALID_NODE);

    // determine node name
    String label_str;
    if (world.any_of<NodeName>(entity)) {
        label_str = world.get_component<NodeName>(entity).name;
    } else {
        label_str = "Node " + std::to_string(static_cast<uint32_t>(entity));
    }
    const char* label = label_str.c_str();

    // filter logic
    bool matches = true;
    if (filter[0] != '\0') {
        String f(filter);
        std::transform(f.begin(), f.end(), f.begin(), ::tolower);
        String n = label_str;
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        matches = (n.find(f) != String::npos);
    }

    if (!matches && is_leaf) return;

    // determine icon
    CString icon = "";
    if (world.any_of<PerspectiveCamera, OrthographicCamera>(entity)) {
        icon = LYRA_ICON_CAMERA;
    } else if (!is_leaf) {
        icon = LYRA_ICON_GROUP;
    } else {
        icon = LYRA_ICON_NODE;
    }

    bool is_selected = (selected_node == node_idx);
    auto on_select   = [&]() {
        selected_node                              = node_idx;
        blackboard.get<HierarchyView::Selection>().node = SceneNode(entity);
    };

    if (is_leaf) {
        ui::tree_leaf(static_cast<uint64_t>(node_idx), icon, label, is_selected, on_select);
    } else {
        ui::tree_item(static_cast<uint64_t>(node_idx), icon, label, is_selected, on_select, [&]() {
            SceneTree::NodeIndex child_idx = node.first_child;
            while (child_idx != SceneTree::INVALID_NODE) {
                render_node(world, hierarchy, child_idx, selected_node, blackboard, filter);
                child_idx = hierarchy.at(child_idx).next_sibling;
            }
        });
    }
}

void HierarchyView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        ui::workspace::dock(LYRA_TREE_VIEW_WINDOW_NAME, ui::Area::Left);
        blackboard.add<Selection>(Selection{});
    });

    ui::panel(LYRA_TREE_VIEW_WINDOW_NAME, [&]() {
        ui::search_bar(search_filter, sizeof(search_filter));
        ui::separator();

        if (auto world_ptr = blackboard.try_get<World*>()) {
            if (auto hierarchy_ptr = blackboard.try_get<SceneTree*>()) {
                auto& world     = **world_ptr;
                auto& hierarchy = **hierarchy_ptr;

                for (auto root_idx : hierarchy) {
                    render_node(world, hierarchy, root_idx, selected_node, blackboard, search_filter);
                }
            }
        }
    });
}
