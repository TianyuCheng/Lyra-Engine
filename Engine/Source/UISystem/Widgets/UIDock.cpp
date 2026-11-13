#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/UISystem/Widgets/UIDock.h>

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::workspace;

namespace
{
    struct WorkspaceNodes
    {
        ImGuiID main   = 0;
        ImGuiID left   = 0;
        ImGuiID right  = 0;
        ImGuiID top    = 0;
        ImGuiID bottom = 0;
    };

    WorkspaceNodes          g_nodes;
    bool                    s_is_building_default_layout = false;
    bool                    s_pending_reset              = false;
    HashMap<String, Area>   s_dock_registrations;

    ImGuiID get_area_node(Area area)
    {
        switch (area) {
            case Area::Main:   return g_nodes.main;
            case Area::Left:   return g_nodes.left ? g_nodes.left : g_nodes.main;
            case Area::Right:  return g_nodes.right ? g_nodes.right : g_nodes.main;
            case Area::Top:    return g_nodes.top ? g_nodes.top : g_nodes.main;
            case Area::Bottom: return g_nodes.bottom ? g_nodes.bottom : g_nodes.main;
        }
        return g_nodes.main;
    }
} // namespace

void lyra::ui::workspace::dockspace(DockSpaceFlags flags, const WorkspaceLayout& default_layout)
{
    ImGuiDockNodeFlags im_flags = ImGuiDockNodeFlags_None;
    if (flags.contains(DockSpaceFlag::PassthruCentralNode)) {
        im_flags |= ImGuiDockNodeFlags_PassthruCentralNode;
    }
    if (flags.contains(DockSpaceFlag::NoDockingInCentralNode)) {
        im_flags |= ImGuiDockNodeFlags_NoDockingInCentralNode;
    }
    if (flags.contains(DockSpaceFlag::NoSplit)) {
        im_flags |= ImGuiDockNodeFlags_NoSplit;
    }
    if (flags.contains(DockSpaceFlag::NoResize)) {
        im_flags |= ImGuiDockNodeFlags_NoResize;
    }
    if (flags.contains(DockSpaceFlag::AutoHideTabBar)) {
        im_flags |= ImGuiDockNodeFlags_AutoHideTabBar;
    }

    ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), im_flags);

    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
    bool needs_build    = s_pending_reset || node == nullptr || (node->ChildNodes[0] == nullptr && node->ChildNodes[1] == nullptr && !node->IsCentralNode());

    if (needs_build) {
        s_is_building_default_layout = true;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, im_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID center_id = dockspace_id;
        ImGuiID left_id   = 0;
        ImGuiID right_id  = 0;
        ImGuiID top_id    = 0;
        ImGuiID bottom_id = 0;

        if (default_layout.top > 0.0f) {
            top_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Up, default_layout.top, nullptr, &center_id);
        }

        if (default_layout.left_full_height) {
            if (default_layout.left > 0.0f) {
                left_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Left, default_layout.left, nullptr, &center_id);
            }
            if (default_layout.bottom > 0.0f) {
                bottom_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Down, default_layout.bottom, nullptr, &center_id);
            }
            if (default_layout.right > 0.0f) {
                right_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, default_layout.right, nullptr, &center_id);
            }
        } else {
            if (default_layout.bottom > 0.0f) {
                bottom_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Down, default_layout.bottom, nullptr, &center_id);
            }
            if (default_layout.left > 0.0f) {
                left_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Left, default_layout.left, nullptr, &center_id);
            }
            if (default_layout.right > 0.0f) {
                right_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, default_layout.right, nullptr, &center_id);
            }
        }

        g_nodes.main   = center_id;
        g_nodes.left   = left_id;
        g_nodes.right  = right_id;
        g_nodes.top    = top_id;
        g_nodes.bottom = bottom_id;

        // dock previously registered panels into their default areas
        for (const auto& [title, area] : s_dock_registrations) {
            ImGuiID target = get_area_node(area);
            if (target != 0) {
                ImGui::DockBuilderDockWindow(title.c_str(), target);
            }
        }

        ImGui::DockBuilderFinish(dockspace_id);
        s_is_building_default_layout = false;
        s_pending_reset              = false;
    }
}

void lyra::ui::workspace::dock_default(CString panel_title, Area area)
{
    if (!panel_title || *panel_title == '\0') {
        return;
    }

    // store association for reset_layout()
    s_dock_registrations[panel_title] = area;

    // if default layout is actively being constructed, dock immediately
    if (s_is_building_default_layout) {
        ImGuiID target = get_area_node(area);
        if (target != 0) {
            ImGui::DockBuilderDockWindow(panel_title, target);
        }
    }
}

void lyra::ui::workspace::reset_layout()
{
    s_pending_reset = true;
}

bool lyra::ui::workspace::is_dockspace_ready()
{
    ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;
    return ImGui::DockBuilderGetNode(dockspace_id) != nullptr;
}
