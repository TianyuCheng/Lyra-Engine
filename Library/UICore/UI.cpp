#include <Lyra/UICore/UI.h>
#include <Lyra/UICore/UIInternals.h>

using namespace lyra;
using namespace lyra::ui;

// =============================================================================
// 1. Panels & Viewport
// =============================================================================

void lyra::ui::panel(CString title, ActionRef content)
{
    internal::reset_layout_counters();
    if (ImGui::Begin(title)) {
        content();
    }
    ImGui::End();
}

void lyra::ui::panel(CString title, bool* p_open, ActionRef content)
{
    internal::reset_layout_counters();
    if (ImGui::Begin(title, p_open)) {
        content();
    }
    ImGui::End();
}

Vector2 lyra::ui::available_space()
{
    ImVec2 s = ImGui::GetContentRegionAvail();
    return Vector2{s.x, s.y};
}

bool lyra::ui::is_panel_appearing()
{
    return ImGui::IsWindowAppearing();
}

bool lyra::ui::is_panel_hovered()
{
    constexpr uint hovered_flags = ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                   ImGuiHoveredFlags_ChildWindows |
                                   ImGuiHoveredFlags_AllowWhenBlockedByPopup;
    return ImGui::IsWindowHovered(hovered_flags);
}

bool lyra::ui::is_panel_focused()
{
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
}

bool lyra::ui::is_any_item_hovered()
{
    return ImGui::IsAnyItemHovered();
}

bool lyra::ui::is_any_item_active()
{
    return ImGui::IsAnyItemActive();
}

// =============================================================================
// 2. Input Queries
// =============================================================================

static ImGuiMouseButton to_imgui_mouse_button(MouseButton button)
{
    // clang-format off
    switch (button) {
        case MouseButton::LEFT:   return ImGuiMouseButton_Left;
        case MouseButton::RIGHT:  return ImGuiMouseButton_Right;
        case MouseButton::MIDDLE: return ImGuiMouseButton_Middle;
        default:                  return ImGuiMouseButton_Left;
    }
    // clang-format on
}

static ImGuiKey to_imgui_key_button(KeyButton button)
{
    // clang-format off
    switch (button) {
        case KeyButton::TAB:           return ImGuiKey_Tab;
        case KeyButton::ESC:           return ImGuiKey_Escape;
        case KeyButton::SPACE:         return ImGuiKey_Space;
        case KeyButton::BACKSPACE:     return ImGuiKey_Backspace;
        case KeyButton::DEL:           return ImGuiKey_Delete;
        case KeyButton::ENTER:         return ImGuiKey_Enter;
        case KeyButton::PAGE_UP:       return ImGuiKey_PageUp;
        case KeyButton::PAGE_DOWN:     return ImGuiKey_PageDown;
        case KeyButton::HOME:          return ImGuiKey_Home;
        case KeyButton::END:           return ImGuiKey_End;
        case KeyButton::PAUSE:         return ImGuiKey_Pause;
        case KeyButton::NUM_LOCK:      return ImGuiKey_NumLock;
        case KeyButton::CAPS_LOCK:     return ImGuiKey_CapsLock;
        case KeyButton::SCROLL_LOCK:   return ImGuiKey_ScrollLock;

        case KeyButton::ALT:           return ImGuiKey_ModAlt;
        case KeyButton::CTRL:          return ImGuiKey_ModCtrl;
        case KeyButton::SHIFT:         return ImGuiKey_ModShift;
        case KeyButton::SUPER:         return ImGuiKey_ModSuper;

        case KeyButton::A:             return ImGuiKey_A;
        case KeyButton::B:             return ImGuiKey_B;
        case KeyButton::C:             return ImGuiKey_C;
        case KeyButton::D:             return ImGuiKey_D;
        case KeyButton::E:             return ImGuiKey_E;
        case KeyButton::F:             return ImGuiKey_F;
        case KeyButton::G:             return ImGuiKey_G;
        case KeyButton::H:             return ImGuiKey_H;
        case KeyButton::I:             return ImGuiKey_I;
        case KeyButton::J:             return ImGuiKey_J;
        case KeyButton::K:             return ImGuiKey_K;
        case KeyButton::L:             return ImGuiKey_L;
        case KeyButton::M:             return ImGuiKey_M;
        case KeyButton::N:             return ImGuiKey_N;
        case KeyButton::O:             return ImGuiKey_O;
        case KeyButton::P:             return ImGuiKey_P;
        case KeyButton::Q:             return ImGuiKey_Q;
        case KeyButton::R:             return ImGuiKey_R;
        case KeyButton::S:             return ImGuiKey_S;
        case KeyButton::T:             return ImGuiKey_T;
        case KeyButton::U:             return ImGuiKey_U;
        case KeyButton::V:             return ImGuiKey_V;
        case KeyButton::W:             return ImGuiKey_W;
        case KeyButton::X:             return ImGuiKey_X;
        case KeyButton::Y:             return ImGuiKey_Y;
        case KeyButton::Z:             return ImGuiKey_Z;

        case KeyButton::UP:            return ImGuiKey_UpArrow;
        case KeyButton::DOWN:          return ImGuiKey_DownArrow;
        case KeyButton::LEFT:          return ImGuiKey_LeftArrow;
        case KeyButton::RIGHT:         return ImGuiKey_RightArrow;

        case KeyButton::D0:            return ImGuiKey_0;
        case KeyButton::D1:            return ImGuiKey_1;
        case KeyButton::D2:            return ImGuiKey_2;
        case KeyButton::D3:            return ImGuiKey_3;
        case KeyButton::D4:            return ImGuiKey_4;
        case KeyButton::D5:            return ImGuiKey_5;
        case KeyButton::D6:            return ImGuiKey_6;
        case KeyButton::D7:            return ImGuiKey_7;
        case KeyButton::D8:            return ImGuiKey_8;
        case KeyButton::D9:            return ImGuiKey_9;

        case KeyButton::F1:            return ImGuiKey_F1;
        case KeyButton::F2:            return ImGuiKey_F2;
        case KeyButton::F3:            return ImGuiKey_F3;
        case KeyButton::F4:            return ImGuiKey_F4;
        case KeyButton::F5:            return ImGuiKey_F5;
        case KeyButton::F6:            return ImGuiKey_F6;
        case KeyButton::F7:            return ImGuiKey_F7;
        case KeyButton::F8:            return ImGuiKey_F8;
        case KeyButton::F9:            return ImGuiKey_F9;
        case KeyButton::F10:           return ImGuiKey_F10;
        case KeyButton::F11:           return ImGuiKey_F11;
        case KeyButton::F12:           return ImGuiKey_F12;

        case KeyButton::APOSTROPHE:    return ImGuiKey_Apostrophe;
        case KeyButton::COMMA:         return ImGuiKey_Comma;
        case KeyButton::MINUS:         return ImGuiKey_Minus;
        case KeyButton::PERIOD:        return ImGuiKey_Period;
        case KeyButton::SLASH:         return ImGuiKey_Slash;
        case KeyButton::BACKSLASH:     return ImGuiKey_Backslash;
        case KeyButton::SEMICOLON:     return ImGuiKey_Semicolon;
        case KeyButton::EQUAL:         return ImGuiKey_Equal;
        case KeyButton::LEFT_BRACKET:  return ImGuiKey_LeftBracket;
        case KeyButton::RIGHT_BRACKET: return ImGuiKey_RightBracket;
        case KeyButton::GRAVE_ACCENT:  return ImGuiKey_GraveAccent;

        default:                       return ImGuiKey_None;
    }
    // clang-format on
}

bool lyra::ui::is_mouse_down(MouseButton button)
{
    return ImGui::IsMouseDown(to_imgui_mouse_button(button));
}

bool lyra::ui::is_mouse_clicked(MouseButton button)
{
    return ImGui::IsMouseClicked(to_imgui_mouse_button(button));
}

bool lyra::ui::is_mouse_released(MouseButton button)
{
    return ImGui::IsMouseReleased(to_imgui_mouse_button(button));
}

bool lyra::ui::is_mouse_double_clicked(MouseButton button)
{
    return ImGui::IsMouseDoubleClicked(to_imgui_mouse_button(button));
}

bool lyra::ui::is_key_down(KeyButton key)
{
    if (key == KeyButton::CTRL) {
        return ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
    }
    if (key == KeyButton::SHIFT) {
        return ImGui::GetIO().KeyShift;
    }
    if (key == KeyButton::ALT) {
        return ImGui::GetIO().KeyAlt;
    }
    if (key == KeyButton::SUPER) {
        return ImGui::GetIO().KeySuper;
    }
    return ImGui::IsKeyDown(to_imgui_key_button(key));
}

bool lyra::ui::is_key_pressed(KeyButton key)
{
    if (key == KeyButton::CTRL) {
        return ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) || ImGui::IsKeyPressed(ImGuiKey_RightCtrl) ||
               ImGui::IsKeyPressed(ImGuiKey_LeftSuper) || ImGui::IsKeyPressed(ImGuiKey_RightSuper);
    }
    if (key == KeyButton::SHIFT) {
        return ImGui::IsKeyPressed(ImGuiKey_LeftShift) || ImGui::IsKeyPressed(ImGuiKey_RightShift);
    }
    if (key == KeyButton::ALT) {
        return ImGui::IsKeyPressed(ImGuiKey_LeftAlt) || ImGui::IsKeyPressed(ImGuiKey_RightAlt);
    }
    if (key == KeyButton::SUPER) {
        return ImGui::IsKeyPressed(ImGuiKey_LeftSuper) || ImGui::IsKeyPressed(ImGuiKey_RightSuper);
    }
    return ImGui::IsKeyPressed(to_imgui_key_button(key));
}

bool lyra::ui::is_key_released(KeyButton key)
{
    if (key == KeyButton::CTRL) {
        return ImGui::IsKeyReleased(ImGuiKey_LeftCtrl) || ImGui::IsKeyReleased(ImGuiKey_RightCtrl) ||
               ImGui::IsKeyReleased(ImGuiKey_LeftSuper) || ImGui::IsKeyReleased(ImGuiKey_RightSuper);
    }
    if (key == KeyButton::SHIFT) {
        return ImGui::IsKeyReleased(ImGuiKey_LeftShift) || ImGui::IsKeyReleased(ImGuiKey_RightShift);
    }
    if (key == KeyButton::ALT) {
        return ImGui::IsKeyReleased(ImGuiKey_LeftAlt) || ImGui::IsKeyReleased(ImGuiKey_RightAlt);
    }
    if (key == KeyButton::SUPER) {
        return ImGui::IsKeyReleased(ImGuiKey_LeftSuper) || ImGui::IsKeyReleased(ImGuiKey_RightSuper);
    }
    return ImGui::IsKeyReleased(to_imgui_key_button(key));
}

// =============================================================================
// 3. Texture & Image Display
// =============================================================================

void lyra::ui::image(GUITextureHandle texture, Vector2 size)
{
    ImGui::Image(as_type<ImTextureID>(texture), ImVec2(size.x, size.y));
}

// =============================================================================
// 4. Application Menus
// =============================================================================

void lyra::ui::menubar(ActionRef content)
{
    if (ImGui::BeginMainMenuBar()) {
        content();
        ImGui::EndMainMenuBar();
    }
}

void lyra::ui::menu(CString title, ActionRef content, bool enabled)
{
    if (ImGui::BeginMenu(title, enabled)) {
        content();
        ImGui::EndMenu();
    }
}

void lyra::ui::menu_item(CString title, CString shortcut, ActionRef on_select)
{
    if (ImGui::MenuItem(title, shortcut)) {
        on_select();
    }
}

void lyra::ui::menu_item(CString title, ActionRef on_select)
{
    if (ImGui::MenuItem(title)) {
        on_select();
    }
}

void lyra::ui::menu_check_item(CString title, bool is_checked, ChangeRef<bool> on_toggle)
{
    if (ImGui::MenuItem(title, nullptr, &is_checked)) {
        on_toggle(is_checked);
    }
}

// =============================================================================
// 5. Context Menus
// =============================================================================

void lyra::ui::item_context_menu(ActionRef content)
{
    if (GImGui->LastItemData.ID == 0) return;
    if (ImGui::BeginPopupContextItem()) {
        content();
        ImGui::EndPopup();
    }
}

void lyra::ui::panel_context_menu(ActionRef content)
{
    if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        content();
        ImGui::EndPopup();
    }
}

// =============================================================================
// 6. Modals
// =============================================================================

void lyra::ui::open_modal(CString id)
{
    ImGui::OpenPopup(id);
}

void lyra::ui::modal(CString id, bool* p_open, ActionRef content)
{
    if (ImGui::BeginPopupModal(id, p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        content();
        ImGui::EndPopup();
    }
}

void lyra::ui::close_modal()
{
    ImGui::CloseCurrentPopup();
}
