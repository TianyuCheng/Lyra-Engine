#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UIDOCK_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UIDOCK_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>

namespace lyra::ui
{
    enum struct DockSpaceFlag : uint32_t
    {
        None                   = 0,
        PassthruCentralNode    = 1 << 0, // transparent central node for 3D viewport rendering
        NoDockingInCentralNode = 1 << 1,
        NoSplit                = 1 << 2,
        NoResize               = 1 << 3,
        AutoHideTabBar         = 1 << 4,
    };
} // namespace lyra::ui

ENABLE_BIT_FLAGS(lyra::ui::DockSpaceFlag);

namespace lyra::ui::workspace
{
    using DockSpaceFlags = BitFlags<DockSpaceFlag>;

    struct WorkspaceLayout
    {
        float left             = 0.20f;
        float right            = 0.25f;
        float bottom           = 0.30f;
        float top              = 0.0f; // 0.0f = no top split
        bool  left_full_height = true; // left sidebar extends full height
    };

    // primary workspace dockspace submission (called every frame in layout layer)
    // constructs default split nodes only if no layout exists in imgui.ini or after reset_layout()
    void dockspace(DockSpaceFlags flags = DockSpaceFlag::PassthruCentralNode, const WorkspaceLayout& default_layout = {});

    // dock window to an area during initial layout construction
    // safe to call every frame: no-op once initial/saved layout is loaded (no execute_once required)
    void dock_default(CString panel_title, Area area);

    // request layout reset to defaults on the next frame (e.g. for "Window -> Reset Layout")
    void reset_layout();

    // query whether the dockspace is currently established
    auto is_dockspace_ready() -> bool;

} // namespace lyra::ui::workspace

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UIDOCK_H
