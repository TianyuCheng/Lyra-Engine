#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/UISystem/UIDock.h>

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::workspace;

namespace
{
    struct WorkspaceState
    {
        ImGuiID main        = 0;
        ImGuiID left        = 0;
        ImGuiID right       = 0;
        ImGuiID top         = 0;
        ImGuiID bottom      = 0;
        bool    initialized = false;
    };

    WorkspaceState g_workspace;
} // namespace

void lyra::ui::workspace::setup(const LayoutSplit& split)
{
    ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());

    if (!g_workspace.initialized) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        g_workspace.main   = dockspace_id;
        g_workspace.top    = ImGui::DockBuilderSplitNode(g_workspace.main, ImGuiDir_Up, split.top, nullptr, &g_workspace.main);
        g_workspace.left   = ImGui::DockBuilderSplitNode(g_workspace.main, ImGuiDir_Left, split.left, nullptr, &g_workspace.main);
        g_workspace.bottom = ImGui::DockBuilderSplitNode(g_workspace.main, ImGuiDir_Down, split.bottom, nullptr, &g_workspace.main);
        g_workspace.right  = ImGui::DockBuilderSplitNode(g_workspace.main, ImGuiDir_Right, split.right, nullptr, &g_workspace.main);

        ImGui::DockBuilderFinish(dockspace_id);
        g_workspace.initialized = true;
    }
}

void lyra::ui::workspace::dock(CString panel_title, Area area)
{
    ImGuiID target_id = 0;

    // clang-format off
    switch (area)
    {
        case Area::Main:   target_id = g_workspace.main; break;
        case Area::Left:   target_id = g_workspace.left; break;
        case Area::Right:  target_id = g_workspace.right; break;
        case Area::Top:    target_id = g_workspace.top; break;
        case Area::Bottom: target_id = g_workspace.bottom; break;
    }
    // clang-format on

    if (target_id != 0) {
        ImGui::DockBuilderDockWindow(panel_title, target_id);
    }
}

LayoutNodes lyra::ui::workspace::get_nodes()
{
    LayoutNodes nodes;
    nodes.main   = static_cast<uint32_t>(g_workspace.main);
    nodes.left   = static_cast<uint32_t>(g_workspace.left);
    nodes.right  = static_cast<uint32_t>(g_workspace.right);
    nodes.top    = static_cast<uint32_t>(g_workspace.top);
    nodes.bottom = static_cast<uint32_t>(g_workspace.bottom);
    return nodes;
}
