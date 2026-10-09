#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UI_INTERNALS_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UI_INTERNALS_H

#include <stack>
#include "ImGui.h"
#include <Lyra/UISystem/Widgets/UIEnums.h>

namespace lyra::ui::internal
{
    struct LayoutScope
    {
        bool      in_row             = false;
        bool      is_first_item      = true;
        Alignment align              = Alignment::Start;
        VAlign    vertical           = VAlign::Center;
        ImGuiID   id                 = 0;
        bool      has_spacer         = false;
        bool      has_spacer_pending = false;
        ImGuiID   spring_id          = 0;
        float     spacer_screen_x    = 0.0f;
    };

    extern thread_local std::stack<LayoutScope> g_layout_stack;
    extern thread_local int                     g_row_counter;

    // advance cursor and apply SameLine() if currently within a ui::row
    void advance_layout_item();

    // check whether the current row has vertical centering or baseline alignment active
    bool is_vertically_centered();

    // reset per-panel layout counters and state
    void reset_layout_counters();

    // retrieve the active imgui window safely
    FORCE_INLINE auto get_current_window() -> ImGuiWindow*
    {
        return ImGui::GetCurrentWindow();
    }

    // retrieve the active imgui context
    FORCE_INLINE auto get_current_context() -> ImGuiContext*
    {
        return GImGui;
    }

} // namespace lyra::ui::internal

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UI_INTERNALS_H
