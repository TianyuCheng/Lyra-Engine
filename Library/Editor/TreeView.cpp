#include <algorithm>
#include <string>

#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Scenes/Camera.h>
#include <Lyra/Scenes/SceneTree.h>
#include <Lyra/Scenes/SceneNode.h>

// local imports
#include <Lyra/Editor/Icons.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/TreeView.h>

#define LYRA_TREE_VIEW_WINDOW_NAME (LYRA_ICON_TREE " Hierarchy")

using namespace lyra;

TreeView::TreeView()
{
    // do nothing
}

void TreeView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &TreeView::update>(*this);
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

    // determine expansion state
    bool expanded = false;
    if (world.any_of<TreeView::Expansion>(entity)) {
        expanded = world.get_component<TreeView::Expansion>(entity).expanded;
    }

    // imgui tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (is_leaf) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selected_node == node_idx) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (expanded || filter[0] != '\0') { // auto-expand when searching
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    // render tree node
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    bool is_open = ImGui::TreeNodeEx((void*)(uintptr_t)node_idx, flags, "%s %s", icon, label);

    // handle selection
    if (ImGui::IsItemClicked()) {
        selected_node                              = node_idx;
        blackboard.get<TreeView::Selection>().node = SceneNode(entity);
    }

    // update expansion state if changed
    if (ImGui::IsItemToggledOpen()) {
        world.add_component<TreeView::Expansion>(entity, TreeView::Expansion{is_open});
    }

    if (is_open && node.first_child != SceneTree::INVALID_NODE) {
        SceneTree::NodeIndex child_idx = node.first_child;
        while (child_idx != SceneTree::INVALID_NODE) {
            render_node(world, hierarchy, child_idx, selected_node, blackboard, filter);
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
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##HierarchySearch", LYRA_ICON_FILTER " Search...", search_filter, sizeof(search_filter));
        ImGui::Separator();

        if (auto world_ptr = blackboard.try_get<World*>()) {
            if (auto hierarchy_ptr = blackboard.try_get<SceneTree*>()) {
                auto& world     = **world_ptr;
                auto& hierarchy = **hierarchy_ptr;

                if (ImGui::BeginTable("##HierarchyTable", 1, ImGuiTableFlags_RowBg)) {
                    for (auto root_idx : hierarchy) {
                        render_node(world, hierarchy, root_idx, selected_node, blackboard, search_filter);
                    }
                    ImGui::EndTable();
                }
            }
        }
    }
    ImGui::End();
}
