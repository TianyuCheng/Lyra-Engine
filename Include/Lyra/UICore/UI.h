#pragma once

#ifndef LYRA_LYRA_UICORE_UI_H
#define LYRA_LYRA_UICORE_UI_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Window/WSIEnums.h>
#include <Lyra/UICore/GUIAPI.h>
#include <Lyra/UICore/UIEnums.h>

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
    auto available_space() -> Vector2;
    auto mouse_pos() -> Vector2;

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

} // namespace lyra::ui

#endif // LYRA_LYRA_UICORE_UI_H
