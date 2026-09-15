#pragma once

#ifndef LYRA_LYRA_UICORE_UI_H
#define LYRA_LYRA_UICORE_UI_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Function.h>
#include <Lyra/UICore/GUIAPI.h>
#include <Lyra/UICore/UIEnums.h>

namespace lyra::ui
{
    // High-performance zero-allocation stack references for synchronous callbacks
    using ActionRef = FunctionRef<void()>;

    template <typename T>
    using ChangeRef = FunctionRef<void(const T&)>;

    struct Rect
    {
        Vector2 min = {0.0f, 0.0f};
        Vector2 max = {0.0f, 0.0f};

        bool overlaps(const Rect& other) const
        {
            return min.x < other.max.x && max.x > other.min.x &&
                   min.y < other.max.y && max.y > other.min.y;
        }
    };

    // =========================================================================
    // 1. Panels (Top-level workspace panels)
    // =========================================================================

    void panel(CString title, ActionRef content);
    void panel(CString title, bool* p_open, ActionRef content);

    // Context & Viewport metrics
    auto available_space() -> Vector2;
    bool is_panel_appearing();
    bool is_panel_hovered();
    bool is_any_item_hovered();
    bool is_any_item_active();
    bool is_mouse_clicked(int button = 0);
    bool is_mouse_released(int button = 0);
    bool is_mouse_down(int button = 0);
    bool is_ctrl_down();
    auto mouse_pos() -> Vector2;
    auto cursor_screen_pos() -> Vector2;
    void draw_selection_rect(Vector2 min, Vector2 max);
    Rect last_item_rect();

    // Texture / Framebuffer display
    void image(GUITextureHandle texture, Vector2 size);

    // =========================================================================
    // 2. Application Menus
    // =========================================================================

    void menubar(ActionRef content);
    void menu(CString title, ActionRef content, bool enabled = true);
    void menu_item(CString title, CString shortcut, ActionRef on_select);
    void menu_item(CString title, ActionRef on_select);
    void menu_check_item(CString title, bool is_checked, ChangeRef<bool> on_toggle);

    // =========================================================================
    // 3. Popups & Modals
    // =========================================================================

    void open_modal(CString title);
    void open_popup(CString id);
    void context_menu(ActionRef content);
    void context_menu(CString id, ActionRef content);
    void window_context_menu(CString id, ActionRef content);
    void modal(CString title, bool* p_open, ActionRef content);
    void close_popup();

} // namespace lyra::ui

#endif // LYRA_LYRA_UICORE_UI_H
