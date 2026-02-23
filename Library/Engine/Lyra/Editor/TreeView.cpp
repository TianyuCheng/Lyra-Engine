#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/SceneNode.h>
#include <fmt/format.h>

// local imports
#include "Icons.h"
#include "Layout.h"
#include "TreeView.h"

#define LYRA_TREE_VIEW_WINDOW_NAME (LYRA_ICON_TREE "Hierarchy")

using namespace lyra;

TreeView::TreeView()
{
}

void TreeView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &TreeView::update>(*this);
}

static void render_node(World& world, SceneTree& hierarchy, SceneTree::NodeIndex node_idx, SceneTree::NodeIndex& selected_node, Blackboard& blackboard)
{
    const auto& node   = hierarchy.at(node_idx);
    const auto  entity = node.entity;

    // get node name
    String label;
    if (world.any_of<NodeName>(entity)) {
        label = world.get_component<NodeName>(entity).name;
    } else {
        label = fmt::format("Node {}", static_cast<uint32_t>(entity));
    }

    // get expansion state
    bool expanded = false;
    if (world.any_of<TreeView::Expansion>(entity)) {
        expanded = world.get_component<TreeView::Expansion>(entity).expanded;
    }

    // imgui tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.first_child == SceneTree::INVALID_NODE) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selected_node == node_idx) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (expanded) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    // render tree node
    bool is_open = ImGui::TreeNodeEx((void*)(uintptr_t)node_idx, flags, "%s", label.c_str());

    // handle selection
    if (ImGui::IsItemClicked()) {
        selected_node = node_idx;
        blackboard.get<TreeView::Selection>().node = SceneNode(entity);
    }

    // update expansion state if changed
    if (ImGui::IsItemToggledOpen()) {
        world.add_component<TreeView::Expansion>(entity, TreeView::Expansion{is_open});
    }

    if (is_open && node.first_child != SceneTree::INVALID_NODE) {
        SceneTree::NodeIndex child_idx = node.first_child;
        while (child_idx != SceneTree::INVALID_NODE) {
            render_node(world, hierarchy, child_idx, selected_node, blackboard);
            child_idx = hierarchy.at(child_idx).next_sibling;
        }
        ImGui::TreePop();
    }
}

void TreeView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_TREE_VIEW_WINDOW_NAME, layout.left);
        blackboard.add<Selection>(Selection{});
    });

    ImGui::Begin(LYRA_TREE_VIEW_WINDOW_NAME);
    {
        if (auto world_ptr = blackboard.try_get<World*>()) {
            if (auto hierarchy_ptr = blackboard.try_get<SceneTree*>()) {
                auto& world     = **world_ptr;
                auto& hierarchy = **hierarchy_ptr;

                for (auto root_idx : hierarchy) {
                    render_node(world, hierarchy, root_idx, selected_node, blackboard);
                }
            }
        }
    }
    ImGui::End();
}
