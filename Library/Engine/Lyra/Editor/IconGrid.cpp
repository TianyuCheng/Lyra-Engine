#include <Lyra/Common/GUI.h>
#include <Lyra/Common/String.h>

#include "IconGrid.h"

using namespace lyra;

auto IconGrid::begin() -> Context
{
    ImGuiStyle& style = ImGui::GetStyle();

    // calculate icon_scale based on icon_size and font metrics
    // icon_size / (ImGui's base font size * ImGui's base font scale)
    float base_font_size    = style.FontSizeBase;
    float font_global_scale = ImGui::GetIO().FontGlobalScale;

    icon_scale = std::floor((icon_size - padding) / (base_font_size * font_global_scale));

    Context ctx;
    ctx.start_x   = ImGui::GetCursorPosX();
    ctx.row_width = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    return ctx;
}

auto IconGrid::draw_item(Context& ctx, CString icon, CString label, bool selected) -> int
{
    int interaction = None;

    const ImVec2 pos = ImGui::GetCursorPos();
    ImGui::BeginGroup();
    {
        // background button for interaction
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Header]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered]);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        }

        ImGui::Button("##bg", ImVec2(icon_size, icon_size));

        if (ImGui::IsItemHovered()) interaction |= Hovered;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) interaction |= Clicked;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) interaction |= DoubleClicked;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) interaction |= RightClicked;

        ImGui::PopStyleColor(selected ? 2 : 1);

        // draw icon centered in the box
        ImGui::SetWindowFontScale(icon_scale);
        const ImVec2 s = ImGui::CalcTextSize(icon);
        ImGui::SetCursorPosX(pos.x + (icon_size - s.x) * 0.5f);
        ImGui::SetCursorPosY(pos.y + (icon_size - s.y) * 0.5f);
        ImGui::TextUnformatted(icon);
        ImGui::SetWindowFontScale(1.0f);

        // draw label under icon
        ImGui::SetCursorPosY(pos.y + icon_size + ImGui::GetStyle().ItemSpacing.y);
        const float w = ImGui::CalcTextSize(label, nullptr, false, icon_size).x;
        ImGui::SetCursorPosX(pos.x + (icon_size > w ? (icon_size - w) * 0.5f : 0));
        ImGui::PushTextWrapPos(pos.x + icon_size);
        ImGui::TextWrapped("%s", label);
        ImGui::PopTextWrapPos();
    }
    ImGui::EndGroup();

    next_column(ctx);
    return interaction;
}

void IconGrid::next_column(Context& ctx)
{
    float last_x = ImGui::GetItemRectMax().x;
    if (last_x + padding + icon_size < ctx.row_width) {
        ImGui::SameLine(0.0f, padding);
    } else {
        ImGui::NewLine();
        ImGui::SetCursorPosX(ctx.start_x);
    }
}
