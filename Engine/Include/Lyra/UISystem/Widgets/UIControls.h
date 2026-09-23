#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UICONTROLS_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UICONTROLS_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>

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

    void text_field(CString id, char* buffer, size_t buffer_size);
    void text_field(CString id, char* buffer, size_t buffer_size, ActionRef on_commit);

    void slider(CString id, float* value, float min, float max, CString format = "%.0f", float width = 0.0f);

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

    // =========================================================================
    // 6. Navigation & Breadcrumbs
    // =========================================================================

    struct BreadcrumbItem
    {
        CString                label    = nullptr;
        CString                icon     = nullptr;
        Function<void() const> on_click = nullptr;
        CString                tooltip  = nullptr;

        BreadcrumbItem() = default;
        BreadcrumbItem(CString label, CString icon = nullptr, Function<void() const> on_click = nullptr, CString tooltip = nullptr)
            : label(label), icon(icon), on_click(std::move(on_click)), tooltip(tooltip)
        {
        }
    };

    void breadcrumb(const BreadcrumbItem* items, size_t count);
    void breadcrumb(const Vector<BreadcrumbItem>& items);
    void breadcrumb(InitList<BreadcrumbItem> items);

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UICONTROLS_H
