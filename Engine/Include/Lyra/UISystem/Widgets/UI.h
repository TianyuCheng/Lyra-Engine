#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UI_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UI_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Windowing/WSIEnums.h>
#include <Lyra/UISystem/Renderer/GUIAPI.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>

namespace lyra::ui
{
    // high-performance zero-allocation stack references for synchronous callbacks
    using ActionRef = FunctionRef<void()>;

    template <typename T>
    using ChangeRef = FunctionRef<void(const T&)>;

    // =========================================================================
    // 1. Panels (Top-level workspace panels)
    // =========================================================================

    void panel(CString title, ActionRef content);
    void panel(CString title, bool* p_open, ActionRef content);

    // panel context & state
    auto get_available_space() -> Vector2;
    auto get_mouse_pos() -> Vector2;

    bool is_panel_appearing();
    bool is_panel_hovered();
    bool is_panel_focused();
    bool is_any_item_hovered();
    bool is_any_item_active();

    // =========================================================================
    // 2. Input Queries
    // =========================================================================

    bool is_mouse_down(MouseButton button = MouseButton::LEFT);
    bool is_mouse_clicked(MouseButton button = MouseButton::LEFT);
    bool is_mouse_released(MouseButton button = MouseButton::LEFT);
    bool is_mouse_double_clicked(MouseButton button = MouseButton::LEFT);

    bool is_key_down(KeyButton key);
    bool is_key_pressed(KeyButton key);
    bool is_key_released(KeyButton key);

    // =========================================================================
    // 3. Texture & Image Display
    // =========================================================================

    void image(GUITextureHandle texture, Vector2 size);

    // =========================================================================
    // 4. Application Menus
    // =========================================================================

    void menubar(ActionRef content);
    void menu(CString title, ActionRef content, bool enabled = true);
    void menu_item(CString title, CString shortcut, ActionRef on_select);
    void menu_item(CString title, ActionRef on_select);
    void menu_check_item(CString title, bool is_checked, ChangeRef<bool> on_toggle);

    // =========================================================================
    // 5. Context Menus (Right-Click Flyouts)
    // =========================================================================

    void item_context_menu(ActionRef content);
    void panel_context_menu(ActionRef content);

    // =========================================================================
    // 6. Modals (Blocking Dialogs)
    // =========================================================================

    void modal(CString id, bool* p_open, ActionRef content);
    void open_modal(CString id);
    void close_modal();

    // =========================================================================
    // 7. Geometry & Item Metrics
    // =========================================================================

    using lyra::Rect;

    // cursor and layout positioning
    auto get_cursor_screen_pos() -> Vector2;
    void set_cursor_screen_pos(Vector2 pos);
    auto get_cursor_pos() -> Vector2;
    void set_cursor_pos(Vector2 pos);

    // item bounds and interaction state (queries the most recent UI item)
    auto get_item_rect() -> Rect;
    auto get_item_rect_min() -> Vector2;
    auto get_item_rect_max() -> Vector2;
    auto get_item_rect_size() -> Vector2;
    bool is_item_hovered();
    bool is_item_clicked(MouseButton button = MouseButton::LEFT);
    bool is_item_active();
    bool is_item_visible();

    // =========================================================================
    // 8. Canvas & 2D Custom Drawing
    // =========================================================================

    // draw primitive shapes on the current panel draw list (screen coordinates)
    void draw_line(Vector2 p1, Vector2 p2, Vector4 color, float thickness = 1.0f);
    void draw_rect(Vector2 min, Vector2 max, Vector4 color, bool filled = false, float rounding = 0.0f, float thickness = 1.0f);
    void draw_circle(Vector2 center, float radius, Vector4 color, bool filled = false, float thickness = 1.0f);
    void draw_selection_rect(Vector2 min, Vector2 max);

    // custom dedicated canvas area with automatic clipping
    void canvas(CString id, Vector2 size, FunctionRef<void(Vector2 origin, Vector2 canvas_size)> content);

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UI_H
