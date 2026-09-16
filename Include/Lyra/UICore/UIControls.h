#pragma once

#ifndef LYRA_LYRA_UICORE_UICONTROLS_H
#define LYRA_LYRA_UICORE_UICONTROLS_H

#include <Lyra/Common/String.h>
#include <Lyra/UICore/UI.h>
#include <Lyra/UICore/UIEnums.h>

namespace lyra::ui
{
    // =========================================================================
    // 1. Action & Icon Buttons
    // =========================================================================

    void button(CString label, ActionRef on_click, ButtonRole role = ButtonRole::Standard);
    void button(CString label, ButtonRole role = ButtonRole::Standard);

    void icon_button(CString icon, ActionRef on_click, CString tooltip = nullptr, ButtonRole role = ButtonRole::Standard);
    void icon_button(CString icon, CString tooltip = nullptr, ButtonRole role = ButtonRole::Standard);

    // =========================================================================
    // 2. Toggle Buttons (e.g. Play/Pause state, Log level filters T/D/I/W/E/C)
    // =========================================================================

    void toggle_button(CString label, bool is_active, ChangeRef<bool> on_toggle, Vector2 size = {0.0f, 0.0f});
    void toggle_button(CString label, bool is_active, StatusRole active_role, ChangeRef<bool> on_toggle, Vector2 size = {0.0f, 0.0f});

    // =========================================================================
    // 3. Form & Search Inputs
    // =========================================================================

    void checkbox(CString label, bool is_checked, ChangeRef<bool> on_toggle);
    void checkbox(CString label, bool is_checked);

    void search_bar(char* buffer, size_t buffer_size, float width = 0.0f, CString hint = nullptr);
    void search_bar(char* buffer, size_t buffer_size, ChangeRef<StringView> on_search, float width = 0.0f, CString hint = nullptr);

    void text_field(CString label, char* buffer, size_t buffer_size);
    void text_field(CString label, char* buffer, size_t buffer_size, ActionRef on_commit);

    void slider(CString label, float* value, float min, float max, CString format = "%.0f", float width = 0.0f);

    // =========================================================================
    // 4. Labels & Badges
    // =========================================================================

    void label(CString text);
    void label(CString text, StatusRole role);
    void badge(CString text, StatusRole role);

    // =========================================================================
    // 5. Selectable Cards (Grid items, asset browsers, pickers)
    // =========================================================================

    void card(CString id, CString icon, CString label, bool is_selected, ActionRef on_click, Vector4 icon_color = Vector4(0.0f), float size = 96.0f);
    void card(CString id, CString icon, CString label, bool is_selected, ActionRef on_click, ActionRef on_double_click, Vector4 icon_color = Vector4(0.0f), float size = 96.0f);
    void card(CString id, GUITextureHandle image, Vector2 image_size, CString label, bool is_selected, ActionRef on_click, float size = 96.0f);
    void card(CString id, GUITextureHandle image, Vector2 image_size, CString label, bool is_selected, ActionRef on_click, ActionRef on_double_click, float size = 96.0f);

} // namespace lyra::ui

#endif // LYRA_LYRA_UICORE_UICONTROLS_H
