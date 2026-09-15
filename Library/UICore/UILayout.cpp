#include <stack>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/UICore/UILayout.h>

using namespace lyra;
using namespace lyra::ui;

namespace
{
    struct LayoutScope
    {
        bool      in_row             = false;
        bool      is_first_item      = true;
        Alignment align              = Alignment::Start;
        ImGuiID   id                 = 0;
        bool      has_spacer         = false;
        bool      has_spacer_pending = false;
        ImGuiID   spring_id          = 0;
        float     spacer_screen_x    = 0.0f;
    };

    thread_local std::stack<LayoutScope> g_layout_stack;
    thread_local int                     g_row_counter = 0;
}

namespace lyra::ui::internal
{
    void advance_layout_item()
    {
        if (g_layout_stack.empty()) return;

        auto& current = g_layout_stack.top();
        if (current.in_row) {
            if (current.has_spacer_pending) {
                current.has_spacer_pending = false;
            } else if (!current.is_first_item) {
                ImGui::SameLine();
            }
            current.is_first_item = false;
        }
    }

    void reset_layout_counters()
    {
        g_row_counter = 0;
    }
}

void lyra::ui::row(ActionRef content)
{
    row(Alignment::Start, content);
}

void lyra::ui::row(Alignment align, ActionRef content)
{
    int row_idx = g_row_counter++;
    ImGuiID row_id = ImGui::GetID(row_idx);
    ImGui::PushID(row_id);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    float prev_width = storage->GetFloat(row_id, 0.0f);

    if (align == Alignment::Center && prev_width > 0.0f) {
        float avail = ImGui::GetContentRegionAvail().x;
        float offset = std::max(0.0f, (avail - prev_width) * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    } else if (align == Alignment::End && prev_width > 0.0f) {
        float avail = ImGui::GetContentRegionAvail().x;
        float offset = std::max(0.0f, avail - prev_width);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    }

    LayoutScope scope;
    scope.in_row        = true;
    scope.is_first_item = true;
    scope.align         = align;
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

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        float max_allowed = window->Pos.x + ImGui::GetWindowContentRegionMax().x;
        if (window->DC.CursorMaxPos.x > max_allowed) {
            window->DC.CursorMaxPos.x = max_allowed;
        }
    }

    ImGui::PopID();
    g_layout_stack.pop();
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

    auto& current = g_layout_stack.top();
    current.has_spacer = true;
    current.has_spacer_pending = true;

    current.spring_id = ImGui::GetID("##LyraUISpacer");
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float post_width = storage->GetFloat(current.spring_id, 0.0f);

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
    current.is_first_item = false;
}

void lyra::ui::separator()
{
    if (g_layout_stack.empty() || !g_layout_stack.top().in_row) {
        ImGui::Separator();
    } else {
        internal::advance_layout_item();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2.0f);
    }
}

void lyra::ui::grid(CString id, float item_width, ActionRef content)
{
    float avail_x = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    int cols = std::max(1, static_cast<int>(avail_x / (item_width + spacing)));

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
