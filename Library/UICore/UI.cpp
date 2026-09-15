#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/UICore/UI.h>

using namespace lyra;
using namespace lyra::ui;

// =============================================================================
// 1. Panels & Viewport
// =============================================================================

namespace lyra::ui::internal
{
    void reset_layout_counters();
}

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

bool lyra::ui::is_any_item_hovered()
{
    return ImGui::IsAnyItemHovered();
}

bool lyra::ui::is_any_item_active()
{
    return ImGui::IsAnyItemActive();
}

bool lyra::ui::is_mouse_clicked(int button)
{
    return ImGui::IsMouseClicked(button);
}

bool lyra::ui::is_mouse_released(int button)
{
    return ImGui::IsMouseReleased(button);
}

bool lyra::ui::is_mouse_down(int button)
{
    return ImGui::IsMouseDown(button);
}

bool lyra::ui::is_ctrl_down()
{
    return ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
}

Vector2 lyra::ui::mouse_pos()
{
    ImVec2 pos = ImGui::GetMousePos();
    return Vector2{pos.x, pos.y};
}

Vector2 lyra::ui::cursor_screen_pos()
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    return Vector2{pos.x, pos.y};
}

void lyra::ui::draw_selection_rect(Vector2 min, Vector2 max)
{
    ImVec2 p_min(std::min(min.x, max.x), std::min(min.y, max.y));
    ImVec2 p_max(std::max(min.x, max.x), std::max(min.y, max.y));
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(p_min, p_max, ImGui::GetColorU32(ImGuiCol_Header, 0.3f));
    draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(ImGuiCol_Header, 1.0f));
}

Rect lyra::ui::last_item_rect()
{
    ImVec2 min_pos = ImGui::GetItemRectMin();
    ImVec2 max_pos = ImGui::GetItemRectMax();
    return Rect{ Vector2{min_pos.x, min_pos.y}, Vector2{max_pos.x, max_pos.y} };
}

void lyra::ui::image(GUITextureHandle texture, Vector2 size)
{
    ImGui::Image(as_type<ImTextureID>(texture), ImVec2(size.x, size.y));
}

// =============================================================================
// 2. Application Menus
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
// 3. Popups & Modals
// =============================================================================

void lyra::ui::open_modal(CString title)
{
    ImGui::OpenPopup(title);
}

void lyra::ui::open_popup(CString id)
{
    ImGui::OpenPopup(id);
}

void lyra::ui::context_menu(ActionRef content)
{
    if (GImGui->LastItemData.ID == 0) return;
    if (ImGui::BeginPopupContextItem()) {
        content();
        ImGui::EndPopup();
    }
}

void lyra::ui::context_menu(CString id, ActionRef content)
{
    bool pushed = false;
    if (id && GImGui->LastItemData.ID != 0) {
        ImGui::PushID(GImGui->LastItemData.ID);
        pushed = true;
    }

    if (ImGui::BeginPopupContextItem(id)) {
        content();
        ImGui::EndPopup();
    }

    if (pushed) {
        ImGui::PopID();
    }
}

void lyra::ui::window_context_menu(CString id, ActionRef content)
{
    if (ImGui::BeginPopupContextWindow(id, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        content();
        ImGui::EndPopup();
    }
}

void lyra::ui::modal(CString title, bool* p_open, ActionRef content)
{
    if (ImGui::BeginPopupModal(title, p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        content();
        ImGui::EndPopup();
    }
}

void lyra::ui::close_popup()
{
    ImGui::CloseCurrentPopup();
}
