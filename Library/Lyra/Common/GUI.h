#pragma once

#ifndef LYRA_LIBRARY_VENDOR_IMGUI_H
#define LYRA_LIBRARY_VENDOR_IMGUI_H

// THIS IS PURELY A HEADER WRAPPER FOR IMGUI.

#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/Common/Macros.h>

namespace imgui
{
    FORCE_INLINE void disable_window_menu_button()
    {
        ImGuiWindowClass window_class;
        window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
        ImGui::SetNextWindowClass(&window_class);
    }
} // namespace imgui

#endif // LYRA_LIBRARY_VENDOR_IMGUI_H
