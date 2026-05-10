#include <Lyra/Common/GUI.h>
#include <Lyra/Common/String.h>

#include <Lyra/Editor/IconGrid.h>

using namespace lyra;

auto IconGrid::begin() -> Context
{
    ImGuiStyle& style = ImGui::GetStyle();

    // calculate icon_scale based on grid_size and font metrics
    // grid_size / (ImGui's base font size * ImGui's base font scale)
    icon_scale = std::floor((grid_size - grid_padding) / style.FontSizeBase);
    icon_scale /= style.FontScaleMain;

    // need to account for additional scaling by monitor DPI
    adjusted_grid_size = grid_size * style.FontScaleMain;

    Context ctx;
    ctx.start_x   = ImGui::GetCursorPosX();
    ctx.row_width = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    return ctx;
}

auto IconGrid::draw_item(Context& ctx, CString icon, CString label, bool selected, ImVec4 icon_color) -> int
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

        ImGui::Button("##bg", ImVec2(adjusted_grid_size, adjusted_grid_size));

        if (ImGui::IsItemHovered()) interaction |= Hovered;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) interaction |= Clicked;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) interaction |= DoubleClicked;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) interaction |= RightClicked;

        ImGui::PopStyleColor(selected ? 2 : 1);

        // draw icon centered in the box
        if (icon_color.w > 0.0f) ImGui::PushStyleColor(ImGuiCol_Text, icon_color);
        ImGui::SetWindowFontScale(icon_scale);
        const ImVec2 s = ImGui::CalcTextSize(icon);
        ImGui::SetCursorPosX(pos.x + (adjusted_grid_size - s.x) * 0.5f);
        ImGui::SetCursorPosY(pos.y + (adjusted_grid_size - s.y) * 0.5f);
        ImGui::TextUnformatted(icon);
        ImGui::SetWindowFontScale(1.0f);
        if (icon_color.w > 0.0f) ImGui::PopStyleColor();

        // draw label under icon
        ImGui::SetCursorPosY(pos.y + adjusted_grid_size + ImGui::GetStyle().ItemSpacing.y);
        const float w = ImGui::CalcTextSize(label, nullptr, false, adjusted_grid_size).x;
        ImGui::SetCursorPosX(pos.x + (adjusted_grid_size > w ? (adjusted_grid_size - w) * 0.5f : 0));
        ImGui::PushTextWrapPos(pos.x + adjusted_grid_size);
        ImGui::TextWrapped("%s", label);
        ImGui::PopTextWrapPos();
    }
    ImGui::EndGroup();

    return interaction;
}

void IconGrid::next_column(Context& ctx)
{
    float last_x = ImGui::GetItemRectMax().x;
    if (last_x + grid_padding + adjusted_grid_size < ctx.row_width) {
        ImGui::SameLine(0.0f, grid_padding);
    } else {
        ImGui::NewLine();
        ImGui::SetCursorPosX(ctx.start_x);
    }
}
