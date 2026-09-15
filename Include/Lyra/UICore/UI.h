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

    // =========================================================================
    // 1. Panels (Top-level workspace panels)
    // =========================================================================

    void panel(CString title, ActionRef content);
    void panel(CString title, bool* p_open, ActionRef content);

    // Context & Viewport metrics
    auto available_space() -> Vector2;
    bool is_panel_appearing();
    bool is_panel_hovered();
    bool is_ctrl_down();

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
