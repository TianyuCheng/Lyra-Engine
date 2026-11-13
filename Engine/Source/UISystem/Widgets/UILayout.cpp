#include <stack>
#include <algorithm>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include "UIInternals.h"

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::internal;

void lyra::ui::row(ActionRef content)
{
    row(RowDescriptor{}, content);
}

void lyra::ui::row(Alignment align, ActionRef content)
{
    RowDescriptor desc;
    desc.align = align;
    row(desc, content);
}

void lyra::ui::row(Alignment align, VAlign vertical, ActionRef content)
{
    RowDescriptor desc;
    desc.align    = align;
    desc.vertical = vertical;
    row(desc, content);
}

void lyra::ui::row(const RowDescriptor& desc, ActionRef content)
{
    int     row_idx = g_row_counter++;
    ImGuiID row_id  = ImGui::GetID(row_idx);
    ImGui::PushID(row_id);

    auto  storage    = ImGui::GetStateStorage();
    float prev_width = storage->GetFloat(row_id, 0.0f);

    if (desc.align == Alignment::Center && prev_width > 0.0f) {
        float avail  = ImGui::GetContentRegionAvail().x;
        float offset = std::max(0.0f, (avail - prev_width) * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    } else if (desc.align == Alignment::End && prev_width > 0.0f) {
        float avail  = ImGui::GetContentRegionAvail().x;
        float offset = std::max(0.0f, avail - prev_width);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    }

    if (desc.vertical == VAlign::Center || desc.vertical == VAlign::Baseline) {
        ImGui::AlignTextToFramePadding();
    }

    LayoutScope scope;
    scope.in_row        = true;
    scope.is_first_item = true;
    scope.align         = desc.align;
    scope.vertical      = desc.vertical;
    scope.id            = row_id;
    g_layout_stack.push(scope);

    ImGui::BeginGroup();
    content();
    ImGui::EndGroup();

    float curr_width = ImGui::GetItemRectSize().x;
    storage->SetFloat(row_id, curr_width);

    if (g_layout_stack.top().has_spacer) {
        float post_width = ImGui::GetItemRectMax().x - g_layout_stack.top().spacer_screen_x;
        if (post_width > 0.0f) {
            storage->SetFloat(g_layout_stack.top().spring_id, post_width);
        }

        auto  window      = ImGui::GetCurrentWindow();
        float max_allowed = window->Pos.x + ImGui::GetWindowContentRegionMax().x;
        if (window->DC.CursorMaxPos.x > max_allowed) {
            window->DC.CursorMaxPos.x = max_allowed;
        }
    }

    ImGui::PopID();
    g_layout_stack.pop();
}

void lyra::ui::align_text_to_frame_padding()
{
    ImGui::AlignTextToFramePadding();
}

void lyra::ui::column(ActionRef content)
{
    LayoutScope scope;
    scope.in_row        = false;
    scope.is_first_item = true;
    scope.align         = Alignment::Start;
    scope.id            = ImGui::GetID("##LyraUIColumn");
    g_layout_stack.push(scope);

    ImGui::BeginGroup();
    content();
    ImGui::EndGroup();

    g_layout_stack.pop();
}

void lyra::ui::toolbar(ActionRef content)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    row(Alignment::Start, content);
    ImGui::PopStyleVar(2);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
}

void lyra::ui::scroll_area(CString id, ActionRef content)
{
    scroll_area(id, 0.0f, content);
}

void lyra::ui::scroll_area(CString id, float reserve_bottom, ActionRef content)
{
    if (ImGui::BeginChild(id, ImVec2(0, -reserve_bottom), false)) {
        content();
    }
    ImGui::EndChild();
}

void lyra::ui::scroll_to_bottom()
{
    ImGui::SetScrollHereY(1.0f);
}

void lyra::ui::spacer()
{
    if (g_layout_stack.empty() || !g_layout_stack.top().in_row) return;

    auto& current              = g_layout_stack.top();
    current.has_spacer         = true;
    current.has_spacer_pending = true;

    current.spring_id        = ImGui::GetID("##LyraUISpacer");
    ImGuiStorage* storage    = ImGui::GetStateStorage();
    float         post_width = storage->GetFloat(current.spring_id, 0.0f);

    if (!current.is_first_item) {
        ImGui::SameLine();
    }

    if (post_width > 0.0f) {
        float right_pos = ImGui::GetWindowContentRegionMax().x - post_width;
        if (right_pos > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(right_pos);
        }
    }

    current.spacer_screen_x = ImGui::GetCursorScreenPos().x;
    current.is_first_item   = false;
}

void lyra::ui::space(float width)
{
    ImGui::SameLine(0.0f, width);
}

void lyra::ui::separator()
{
    if (g_layout_stack.empty() || !g_layout_stack.top().in_row) {
        ImGui::Separator();
    } else {
        internal::advance_layout_item();
        auto  window  = ImGui::GetCurrentWindow();
        float frame_h = ImGui::GetFrameHeight();
        float pad_x   = 4.0f;
        float x       = window->DC.CursorPos.x + pad_x;
        float y_top   = window->DC.CursorPos.y + 3.0f;
        float y_bot   = window->DC.CursorPos.y + frame_h - 3.0f;

        const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + pad_x * 2.0f + 1.0f, window->DC.CursorPos.y + frame_h));
        ImGui::ItemSize(bb);
        if (ImGui::ItemAdd(bb, 0)) {
            window->DrawList->AddLine(ImVec2(x, y_top), ImVec2(x, y_bot), ImGui::GetColorU32(ImGuiCol_Separator));
        }
    }
}

void lyra::ui::grid(CString id, float item_width, ActionRef content)
{
    constexpr float padding_x = 8.0f;
    constexpr float padding_y = 8.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padding_x);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding_y);

    float avail_x = ImGui::GetContentRegionAvail().x - padding_x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    int   cols    = std::max(1, static_cast<int>(avail_x / (item_width + spacing)));

    if (ImGui::BeginTable(id, cols, ImGuiTableFlags_SizingFixedFit)) {
        content();
        ImGui::EndTable();
    }
}

void lyra::ui::grid_item(ActionRef content)
{
    ImGui::TableNextColumn();
    content();
}

void lyra::ui::split_pane(CString id, float& split_width, ActionRef left_pane, ActionRef right_pane, float min_left, float min_right, float reserve_bottom)
{
    internal::advance_layout_item();

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImGuiID splitter_id = window->GetID(id);
    float   avail_w     = ImGui::GetContentRegionAvail().x;
    float   avail_h     = std::max(20.0f, ImGui::GetContentRegionAvail().y - reserve_bottom);

    float splitter_w = 4.0f;

    float max_left = std::max(min_left, avail_w - min_right - splitter_w);
    split_width    = std::clamp(split_width, min_left, max_left);
    float right_w  = std::max(min_right, avail_w - split_width - splitter_w);

    ImVec2 start_pos = window->DC.CursorPos;

    // 1. left pane child
    char left_child_id[64];
    snprintf(left_child_id, sizeof(left_child_id), "%s_Left", id);
    if (ImGui::BeginChild(left_child_id, ImVec2(split_width, avail_h), false)) {
        left_pane();
    }
    ImGui::EndChild();

    // 2. splitter bar: thin, cohesive inner divider with subtle gutter and bright hover highlight
    ImVec2 splitter_pos = ImVec2(start_pos.x + split_width, start_pos.y);
    ImRect splitter_bb(splitter_pos, ImVec2(splitter_pos.x + splitter_w, splitter_pos.y + avail_h));

    float size1 = split_width;
    float size2 = right_w;
    ImU32 bg_col = ImGui::GetColorU32(ImGuiCol_WindowBg);
    bool held = ImGui::SplitterBehavior(splitter_bb, splitter_id, ImGuiAxis_X, &size1, &size2, min_left, min_right, 4.0f, 0.04f, bg_col);
    split_width = size1;

    bool is_hovered = ImGui::IsItemHovered();
    ImDrawList* draw_list = window->DrawList;
    if (bg_col & IM_COL32_A_MASK) {
        draw_list->AddRectFilled(splitter_bb.Min, splitter_bb.Max, bg_col, 0.0f);
    }

    float mid_x = std::round(splitter_bb.Min.x + splitter_w * 0.5f);
    ImU32 line_col = held ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
                          : (is_hovered ? ImGui::GetColorU32(ImGuiCol_SeparatorHovered)
                                        : ImGui::GetColorU32(ImGuiCol_Separator));
    draw_list->AddLine(ImVec2(mid_x, splitter_bb.Min.y), ImVec2(mid_x, splitter_bb.Max.y), line_col, is_hovered || held ? splitter_w : 1.5f);

    // 3. right pane child: position cursor strictly after the splitter
    ImVec2 right_pos = ImVec2(splitter_pos.x + splitter_w, start_pos.y);
    ImGui::SetCursorScreenPos(right_pos);

    char right_child_id[64];
    snprintf(right_child_id, sizeof(right_child_id), "%s_Right", id);
    if (ImGui::BeginChild(right_child_id, ImVec2(right_w, avail_h), false)) {
        right_pane();
    }
    ImGui::EndChild();

    // advance cursor below both panes
    ImGui::SetCursorScreenPos(ImVec2(start_pos.x, start_pos.y + avail_h + ImGui::GetStyle().ItemSpacing.y));
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, start_pos.y + avail_h);
}
