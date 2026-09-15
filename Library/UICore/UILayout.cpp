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
        bool      in_row        = false;
        bool      is_first_item = true;
        Alignment align         = Alignment::Start;
        ImGuiID   id            = 0;
    };

    thread_local std::stack<LayoutScope> g_layout_stack;
}

namespace lyra::ui::internal
{
    void advance_layout_item()
    {
        if (g_layout_stack.empty()) return;

        auto& current = g_layout_stack.top();
        if (current.in_row) {
            if (!current.is_first_item) {
                ImGui::SameLine();
            }
            current.is_first_item = false;
        }
    }
}

void lyra::ui::row(ActionRef content)
{
    row(Alignment::Start, content);
}

void lyra::ui::row(Alignment align, ActionRef content)
{
    ImGuiID row_id = ImGui::GetID("##LyraUIRow");
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
    row(Alignment::Start, content);
    ImGui::PopStyleVar(2);
}

void lyra::ui::scroll_area(CString id, ActionRef content)
{
    if (ImGui::BeginChild(id, ImVec2(0, 0), false)) {
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

    ImGuiID spring_id = ImGui::GetID("##LyraUISpacer");
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float post_width = storage->GetFloat(spring_id, 0.0f);

    if (post_width > 0.0f) {
        float right_pos = ImGui::GetWindowContentRegionMax().x - post_width;
        if (right_pos > ImGui::GetCursorPosX()) {
            ImGui::SameLine(right_pos);
        } else {
            ImGui::SameLine();
        }
    } else {
        ImGui::SameLine();
    }

    g_layout_stack.top().is_first_item = false;
}

void lyra::ui::separator()
{
    if (g_layout_stack.empty() || !g_layout_stack.top().in_row) {
        ImGui::Separator();
    } else {
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        g_layout_stack.top().is_first_item = false;
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
