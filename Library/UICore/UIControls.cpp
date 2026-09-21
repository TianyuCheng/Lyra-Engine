#include <Lyra/UICore/UIIcons.h>
#include <Lyra/UICore/UIControls.h>
#include <Lyra/UICore/UIInternals.h>

using namespace lyra;
using namespace lyra::ui;

namespace
{
    ImVec4 to_status_color(StatusRole role)
    {
        switch (role) {
            case StatusRole::Muted:
                return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            case StatusRole::Trace:
                return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            case StatusRole::Debug:
                return ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
            case StatusRole::Info:
                return ImVec4(0.2f, 0.7f, 1.0f, 1.0f);
            case StatusRole::Success:
                return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
            case StatusRole::Warning:
                return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            case StatusRole::Error:
                return ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
            case StatusRole::Critical:
                return ImVec4(1.0f, 0.0f, 0.8f, 1.0f);
            case StatusRole::Default:
            default:
                return ImGui::GetStyle().Colors[ImGuiCol_Text];
        }
    }

    void apply_button_role(ButtonRole role)
    {
        switch (role) {
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
} // namespace

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

void lyra::ui::toggle_button(CString label, bool is_active, ChangeRef<bool> on_toggle, Vector2 size)
{
    internal::advance_layout_item();
    if (is_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }
    if (ImGui::Button(label, ImVec2(size.x, size.y))) {
        on_toggle(!is_active);
    }
    if (is_active) {
        ImGui::PopStyleColor();
    }
}

void lyra::ui::toggle_button(CString label, bool is_active, StatusRole active_role, ChangeRef<bool> on_toggle, Vector2 size)
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

    if (ImGui::Button(label, ImVec2(size.x, size.y))) {
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

void lyra::ui::search_bar(char* buffer, size_t buffer_size, float width, CString hint)
{
    internal::advance_layout_item();
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    } else {
        ImGui::SetNextItemWidth(-1.0f);
    }
    const char* hint_str = hint ? hint : (LYRA_ICON_FILTER " Search...");
    ImGui::InputTextWithHint("##LyraSearch", hint_str, buffer, buffer_size);
}

void lyra::ui::search_bar(char* buffer, size_t buffer_size, ChangeRef<StringView> on_search, float width, CString hint)
{
    internal::advance_layout_item();
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    } else {
        ImGui::SetNextItemWidth(-1.0f);
    }
    const char* hint_str = hint ? hint : (LYRA_ICON_FILTER " Search...");
    if (ImGui::InputTextWithHint("##LyraSearch", hint_str, buffer, buffer_size)) {
        on_search(StringView(buffer));
    }
}

void lyra::ui::text_field(CString id, char* buffer, size_t buffer_size)
{
    internal::advance_layout_item();
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##text_input", buffer, buffer_size);
    ImGui::PopID();
}

void lyra::ui::text_field(CString id, char* buffer, size_t buffer_size, ActionRef on_commit)
{
    internal::advance_layout_item();
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##text_input", buffer, buffer_size, ImGuiInputTextFlags_EnterReturnsTrue)) {
        on_commit();
    }
    ImGui::PopID();
}

void lyra::ui::slider(CString id, float* value, float min, float max, CString format, float width)
{
    if (!value) return;
    internal::advance_layout_item();

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImGuiContext& g         = *GImGui;
    const ImGuiID widget_id = window->GetID(id);

    float w       = width > 0.0f ? width : 100.0f;
    float frame_h = ImGui::GetFrameHeight();

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + frame_h));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, widget_id)) return;

    ImRect hit_bb = bb;
    hit_bb.Expand(ImVec2(0.0f, 2.0f));

    bool hovered = false;
    bool held    = false;
    ImGui::ButtonBehavior(hit_bb, widget_id, &hovered, &held, ImGuiButtonFlags_None);

    float grab_radius = (held || hovered) ? 6.0f : 5.0f;
    float track_start = bb.Min.x + 6.0f;
    float track_end   = bb.Max.x - 6.0f;

    if (held) {
        float mouse_x = g.IO.MousePos.x;
        float t       = (track_end > track_start) ? std::clamp((mouse_x - track_start) / (track_end - track_start), 0.0f, 1.0f) : 0.0f;
        *value        = min + t * (max - min);
        ImGui::MarkItemEdited(widget_id);
    }

    float mid_y = bb.Min.y + frame_h * 0.5f;
    float norm  = (max > min) ? std::clamp((*value - min) / (max - min), 0.0f, 1.0f) : 0.0f;
    float dot_x = track_start + norm * (track_end - track_start);

    ImDrawList* draw_list = window->DrawList;

    // 1. Inactive background track (thin rounded bar)
    float track_h  = 3.0f;
    ImU32 track_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    draw_list->AddRectFilled(
        ImVec2(track_start, mid_y - track_h * 0.5f),
        ImVec2(track_end, mid_y + track_h * 0.5f),
        track_bg,
        track_h * 0.5f);

    // 2. Active filled track (from start to dot)
    ImU32 active_col = ImGui::GetColorU32(held ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
    draw_list->AddRectFilled(
        ImVec2(track_start, mid_y - track_h * 0.5f),
        ImVec2(dot_x, mid_y + track_h * 0.5f),
        active_col,
        track_h * 0.5f);

    // 3. Dot / Grab handle
    ImU32 dot_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.94f, 0.97f, 1.0f));
    if (held) {
        dot_col = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    draw_list->AddCircleFilled(ImVec2(dot_x, mid_y), grab_radius, dot_col, 16);
    draw_list->AddCircle(ImVec2(dot_x, mid_y), grab_radius, ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.1f, 0.12f, 0.5f)), 16, 1.0f);

    // 4. Value tooltip on hover or drag
    if (hovered || held) {
        if (format && format[0] != '\0') {
            char val_str[64];
            snprintf(val_str, sizeof(val_str), format, *value);
            ImGui::SetTooltip("%s", val_str);
        }
    }
}

// =============================================================================
// 4. Labels & Badges
// =============================================================================

void lyra::ui::label(CString text)
{
    internal::advance_layout_item();
    if (internal::is_vertically_centered()) {
        ImGui::AlignTextToFramePadding();
    }
    ImGui::TextUnformatted(text);
}

void lyra::ui::label(CString text, StatusRole role)
{
    internal::advance_layout_item();
    if (internal::is_vertically_centered()) {
        ImGui::AlignTextToFramePadding();
    }
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

    void finish_card(const ImVec2& pos, float size, CString label, bool is_selected, ActionRef on_click, const ActionRef* on_double_click, ImGuiID card_id)
    {
        if (label && label[0] != '\0') {
            const char* text_begin = label;
            const char* text_end   = label + strlen(label);
            float       line_y     = pos.y + size + 4.0f;
            ImFont*     font       = ImGui::GetFont();
            float       font_size  = ImGui::GetFontSize();

            while (text_begin < text_end) {
                const char* line_end = font->CalcWordWrapPosition(font_size, text_begin, text_end, size);
                if (line_end == text_begin) {
                    line_end = text_begin + 1;
                }
                ImVec2 line_sz = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, text_begin, line_end);
                float  line_x  = pos.x + std::max(0.0f, (size - line_sz.x) * 0.5f);
                ImGui::SetCursorScreenPos(ImVec2(line_x, line_y));
                ImGui::TextUnformatted(text_begin, line_end);
                line_y += font_size + ImGui::GetStyle().ItemSpacing.y;
                text_begin = line_end;
                while (text_begin < text_end && (*text_begin == ' ' || *text_begin == '\t' || *text_begin == '\n' || *text_begin == '\r')) {
                    text_begin++;
                }
            }
        }

        ImGui::EndGroup();
        ImGui::PopID();

        // clicking anywhere on the card group (icon or label) triggers selection / double click
        if (on_double_click && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            (*on_double_click)();
        } else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            on_click();
        }

        GImGui->LastItemData.ID = card_id;
    }

    void draw_card_icon(const ImVec2& pos, float size, CString icon, const Vector4& icon_color)
    {
        float base_font_size = ImGui::GetFontSize();
        float icon_scale     = base_font_size > 0.0f ? std::max(1.0f, (size * 0.72f) / base_font_size) : 4.5f;

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
} // namespace

void lyra::ui::card(CString id, CString icon, CString label, bool is_selected, ActionRef on_click, Vector4 icon_color, float size)
{
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, nullptr);
    ImVec2  pos     = ImGui::GetItemRectMin();

    draw_card_icon(pos, size, icon, icon_color);

    finish_card(pos, size, label, is_selected, on_click, nullptr, card_id);
}

void lyra::ui::card(CString id, CString icon, CString label, bool is_selected, ActionRef on_click, ActionRef on_double_click, Vector4 icon_color, float size)
{
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, &on_double_click);
    ImVec2  pos     = ImGui::GetItemRectMin();

    draw_card_icon(pos, size, icon, icon_color);

    finish_card(pos, size, label, is_selected, on_click, &on_double_click, card_id);
}

void lyra::ui::card(CString id, GUITextureHandle image, Vector2 image_size, CString label, bool is_selected, ActionRef on_click, float size)
{
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, nullptr);
    ImVec2  pos     = ImGui::GetItemRectMin();

    float  max_thumb    = size - 8.0f;
    ImVec2 display_size = {max_thumb, max_thumb};
    if (image_size.x > 0.0f && image_size.y > 0.0f) {
        float aspect = image_size.x / image_size.y;
        if (aspect > 1.0f)
            display_size.y = max_thumb / aspect;
        else
            display_size.x = max_thumb * aspect;
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + (size - display_size.x) * 0.5f, pos.y + (size - display_size.y) * 0.5f));
    ImGui::Image(as_type<ImTextureID>(image), display_size);

    finish_card(pos, size, label, is_selected, on_click, nullptr, card_id);
}

void lyra::ui::card(CString id, GUITextureHandle image, Vector2 image_size, CString label, bool is_selected, ActionRef on_click, ActionRef on_double_click, float size)
{
    ImGuiID card_id = draw_card_frame(id, is_selected, size, on_click, &on_double_click);
    ImVec2  pos     = ImGui::GetItemRectMin();

    float  max_thumb    = size - 8.0f;
    ImVec2 display_size = {max_thumb, max_thumb};
    if (image_size.x > 0.0f && image_size.y > 0.0f) {
        float aspect = image_size.x / image_size.y;
        if (aspect > 1.0f)
            display_size.y = max_thumb / aspect;
        else
            display_size.x = max_thumb * aspect;
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + (size - display_size.x) * 0.5f, pos.y + (size - display_size.y) * 0.5f));
    ImGui::Image(as_type<ImTextureID>(image), display_size);

    finish_card(pos, size, label, is_selected, on_click, &on_double_click, card_id);
}

// =============================================================================
// 6. Navigation & Breadcrumbs
// =============================================================================

void lyra::ui::breadcrumb(const BreadcrumbItem* items, size_t count)
{
    if (!items || count == 0) return;

    internal::advance_layout_item();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(1); // Channel 1: foreground items

    ImGui::BeginGroup();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.09f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.16f));

    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.52f, 0.85f), LYRA_ICON_CARET);
            ImGui::SameLine(0.0f, 2.0f);
        }

        const auto& item    = items[i];
        bool        is_last = (i == count - 1);

        String text;
        if (item.icon && item.icon[0] != '\0') {
            text = String(item.icon) + " " + (item.label ? item.label : "");
        } else {
            text = item.label ? item.label : "";
        }

        ImGui::PushID(static_cast<int>(i));
        if (is_last) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.07f));
            if (ImGui::Button(text.c_str())) {
                if (item.on_click) item.on_click();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.78f, 0.82f, 1.0f));
            if (ImGui::Button(text.c_str())) {
                if (item.on_click) item.on_click();
            }
            ImGui::PopStyleColor();
        }

        if (item.tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", item.tooltip);
        }
        ImGui::PopID();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);

    ImGui::EndGroup();

    // group bounding box
    ImVec2 bb_min = ImGui::GetItemRectMin();
    ImVec2 bb_max = ImGui::GetItemRectMax();

    // background capsule in channel 0
    draw_list->ChannelsSetCurrent(0);

    bb_min.x -= 2.0f;
    bb_max.x += 2.0f;
    bb_min.y -= 1.0f;
    bb_max.y += 1.0f;

    ImU32 bg_col     = ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.14f, 0.70f));
    ImU32 border_col = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    draw_list->AddRectFilled(bb_min, bb_max, bg_col, 5.0f);
    draw_list->AddRect(bb_min, bb_max, border_col, 5.0f);

    draw_list->ChannelsMerge();
}

void lyra::ui::breadcrumb(const Vector<BreadcrumbItem>& items)
{
    breadcrumb(items.data(), items.size());
}

void lyra::ui::breadcrumb(InitList<BreadcrumbItem> items)
{
    breadcrumb(items.begin(), items.size());
}
