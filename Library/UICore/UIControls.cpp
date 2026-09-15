#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/UICore/UIControls.h>

using namespace lyra;
using namespace lyra::ui;

namespace lyra::ui::internal
{
    extern void advance_layout_item();
}

namespace
{
    ImVec4 to_status_color(StatusRole role)
    {
        switch (role)
        {
            case StatusRole::Muted:    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            case StatusRole::Trace:    return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            case StatusRole::Debug:    return ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
            case StatusRole::Info:     return ImVec4(0.2f, 0.7f, 1.0f, 1.0f);
            case StatusRole::Success:  return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
            case StatusRole::Warning:  return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            case StatusRole::Error:    return ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
            case StatusRole::Critical: return ImVec4(1.0f, 0.0f, 0.8f, 1.0f);
            case StatusRole::Default:
            default:                   return ImGui::GetStyle().Colors[ImGuiCol_Text];
        }
    }

    void apply_button_role(ButtonRole role)
    {
        switch (role)
        {
            case ButtonRole::Primary:
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.75f, 1.0f));
                break;
            case ButtonRole::Success:
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.6f, 0.15f, 1.0f));
                break;
            case ButtonRole::Warning:
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.05f, 1.0f));
                break;
            case ButtonRole::Danger:
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
                break;
            case ButtonRole::Ghost:
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
                break;
            case ButtonRole::Standard:
            default:
                break;
        }
    }

    void unapply_button_role(ButtonRole role)
    {
        if (role != ButtonRole::Standard) {
            ImGui::PopStyleColor(3);
        }
    }
}

// =============================================================================
// 1. Action & Icon Buttons
// =============================================================================

void lyra::ui::button(CString label, ActionRef on_click, ButtonRole role)
{
    internal::advance_layout_item();
    apply_button_role(role);
    if (ImGui::Button(label)) {
        on_click();
    }
    unapply_button_role(role);
}

void lyra::ui::button(CString label, ButtonRole role)
{
    internal::advance_layout_item();
    apply_button_role(role);
    ImGui::Button(label);
    unapply_button_role(role);
}

void lyra::ui::icon_button(CString icon, ActionRef on_click, CString tooltip, ButtonRole role)
{
    internal::advance_layout_item();
    apply_button_role(role);
    float sz = ImGui::GetFrameHeight();
    if (ImGui::Button(icon, ImVec2(sz, sz))) {
        on_click();
    }
    unapply_button_role(role);

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void lyra::ui::icon_button(CString icon, CString tooltip, ButtonRole role)
{
    internal::advance_layout_item();
    apply_button_role(role);
    float sz = ImGui::GetFrameHeight();
    ImGui::Button(icon, ImVec2(sz, sz));
    unapply_button_role(role);

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

// =============================================================================
// 2. Toggle Buttons
// =============================================================================

void lyra::ui::toggle_button(CString label, bool is_active, ChangeRef<bool> on_toggle)
{
    internal::advance_layout_item();
    if (is_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }
    if (ImGui::Button(label)) {
        on_toggle(!is_active);
    }
    if (is_active) {
        ImGui::PopStyleColor();
    }
}

void lyra::ui::toggle_button(CString label, bool is_active, StatusRole active_role, ChangeRef<bool> on_toggle)
{
    internal::advance_layout_item();
    ImVec4 col = to_status_color(active_role);

    if (is_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x * 1.15f, col.y * 1.15f, col.z * 1.15f, col.w));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x * 0.85f, col.y * 0.85f, col.z * 0.85f, col.w));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, col);
    }

    if (ImGui::Button(label)) {
        on_toggle(!is_active);
    }

    if (is_active) {
        ImGui::PopStyleColor(4);
    } else {
        ImGui::PopStyleColor(1);
    }
}

// =============================================================================
// 3. Form & Search Inputs
// =============================================================================

void lyra::ui::checkbox(CString label, bool is_checked, ChangeRef<bool> on_toggle)
{
    internal::advance_layout_item();
    bool val = is_checked;
    if (ImGui::Checkbox(label, &val)) {
        on_toggle(val);
    }
}

void lyra::ui::checkbox(CString label, bool is_checked)
{
    internal::advance_layout_item();
    bool val = is_checked;
    ImGui::Checkbox(label, &val);
}

void lyra::ui::search_bar(char* buffer, size_t buffer_size, float width)
{
    internal::advance_layout_item();
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }
    ImGui::InputTextWithHint("##LyraSearch", "Search...", buffer, buffer_size);
}

void lyra::ui::search_bar(char* buffer, size_t buffer_size, ChangeRef<StringView> on_search, float width)
{
    internal::advance_layout_item();
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }
    if (ImGui::InputTextWithHint("##LyraSearch", "Search...", buffer, buffer_size)) {
        on_search(StringView(buffer));
    }
}

void lyra::ui::text_field(CString label, char* buffer, size_t buffer_size)
{
    internal::advance_layout_item();
    if (label && label[0] == '#' && label[1] == '#') {
        ImGui::SetNextItemWidth(-1.0f);
    }
    ImGui::InputText(label, buffer, buffer_size);
}

void lyra::ui::text_field(CString label, char* buffer, size_t buffer_size, ActionRef on_commit)
{
    internal::advance_layout_item();
    if (label && label[0] == '#' && label[1] == '#') {
        ImGui::SetNextItemWidth(-1.0f);
    }
    if (ImGui::InputText(label, buffer, buffer_size, ImGuiInputTextFlags_EnterReturnsTrue)) {
        on_commit();
    }
}

// =============================================================================
// 4. Labels & Badges
// =============================================================================

void lyra::ui::label(CString text)
{
    internal::advance_layout_item();
    ImGui::TextUnformatted(text);
}

void lyra::ui::label(CString text, StatusRole role)
{
    internal::advance_layout_item();
    ImGui::TextColored(to_status_color(role), "%s", text);
}

void lyra::ui::badge(CString text, StatusRole role)
{
    internal::advance_layout_item();
    ImVec4 col = to_status_color(role);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::SmallButton(text);
    ImGui::PopStyleColor(2);
}

// =============================================================================
// 5. Selectable Cards
// =============================================================================

namespace
{
    ImGuiID draw_card_frame(CString id, bool is_selected, float size, ActionRef on_click, const ActionRef* on_double_click)
    {
        ImGuiID card_id = ImGui::GetID(id);
        ImGui::PushID(card_id);
        ImGui::BeginGroup();

        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Header]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered]);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        }

        ImGui::Button("##card_bg", ImVec2(size, size));

        if (on_double_click && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            (*on_double_click)();
        } else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            on_click();
        }

        ImGui::PopStyleColor(is_selected ? 2 : 1);
        return card_id;
    }

    void finish_card(const ImVec2& pos, float size, CString label, ImGuiID card_id)
    {
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size + 2.0f));
        ImGui::PushTextWrapPos(pos.x + size);
        ImGui::TextWrapped("%s", label);
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();

        GImGui->LastItemData.ID = card_id;
    }

    void draw_card_icon(const ImVec2& pos, float size, CString icon, const Vector4& icon_color)
    {
        float base_font_size = ImGui::GetFontSize();
        float icon_scale = base_font_size > 0.0f ? std::max(1.0f, (size * 0.5f) / base_font_size) : 3.0f;

        if (icon_color.w > 0.0f) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(icon_color.x, icon_color.y, icon_color.z, icon_color.w));
        }
        ImGui::SetWindowFontScale(icon_scale);
        const ImVec2 icon_size = ImGui::CalcTextSize(icon);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + (size - icon_size.x) * 0.5f, pos.y + (size - icon_size.y) * 0.5f));
        ImGui::TextUnformatted(icon);
        ImGui::SetWindowFontScale(1.0f);
        if (icon_color.w > 0.0f) {
            ImGui::PopStyleColor();
        }
    }
}

void lyra::ui::card(CString id, CString icon, CString label, bool is_selected, ActionRef on_click, Vector4 icon_color)
{
    float size = 96.0f;
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, nullptr);
    const ImVec2 pos = ImGui::GetItemRectMin();

    draw_card_icon(pos, size, icon, icon_color);

    finish_card(pos, size, label, card_id);
}

void lyra::ui::card(CString id, CString icon, CString label, bool is_selected, ActionRef on_click, ActionRef on_double_click, Vector4 icon_color)
{
    float size = 96.0f;
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, &on_double_click);
    const ImVec2 pos = ImGui::GetItemRectMin();

    draw_card_icon(pos, size, icon, icon_color);

    finish_card(pos, size, label, card_id);
}

void lyra::ui::card(CString id, GUITextureHandle image, Vector2 image_size, CString label, bool is_selected, ActionRef on_click)
{
    float size = 96.0f;
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, nullptr);
    const ImVec2 pos = ImGui::GetItemRectMin();

    float max_thumb = size - 8.0f;
    ImVec2 display_size = { max_thumb, max_thumb };
    if (image_size.x > 0.0f && image_size.y > 0.0f) {
        float aspect = image_size.x / image_size.y;
        if (aspect > 1.0f) display_size.y = max_thumb / aspect;
        else display_size.x = max_thumb * aspect;
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + (size - display_size.x) * 0.5f, pos.y + (size - display_size.y) * 0.5f));
    ImGui::Image(as_type<ImTextureID>(image), display_size);

    finish_card(pos, size, label, card_id);
}

void lyra::ui::card(CString id, GUITextureHandle image, Vector2 image_size, CString label, bool is_selected, ActionRef on_click, ActionRef on_double_click)
{
    float size = 96.0f;
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, &on_double_click);
    const ImVec2 pos = ImGui::GetItemRectMin();

    float max_thumb = size - 8.0f;
    ImVec2 display_size = { max_thumb, max_thumb };
    if (image_size.x > 0.0f && image_size.y > 0.0f) {
        float aspect = image_size.x / image_size.y;
        if (aspect > 1.0f) display_size.y = max_thumb / aspect;
        else display_size.x = max_thumb * aspect;
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + (size - display_size.x) * 0.5f, pos.y + (size - display_size.y) * 0.5f));
    ImGui::Image(as_type<ImTextureID>(image), display_size);

    finish_card(pos, size, label, card_id);
}
