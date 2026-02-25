#pragma once

#ifndef LYRA_LIBRARY_COMMON_GUI_H
#define LYRA_LIBRARY_COMMON_GUI_H

// THIS IS PURELY A HEADER WRAPPER FOR EnTT.

#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/Common/Macros.h>

namespace lyra::imgui
{

    FORCE_INLINE void disable_window_menu_button()
    {
        ImGuiWindowClass window_class;
        window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
        ImGui::SetNextWindowClass(&window_class);
    }

} // namespace lyra::imgui

#endif // LYRA_LIBRARY_COMMON_GUI_H
