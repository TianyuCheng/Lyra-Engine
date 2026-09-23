#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/UISystem/Widgets/UIProperty.h>

using namespace lyra;
using namespace lyra::ui;

namespace
{
    void begin_property_row(CString label)
    {
        if (GImGui->CurrentTable != nullptr) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine();
        }
    }

    bool draw_vec_row_internal(CString label, float* values, int count, const VecConfig& config)
    {
        bool changed = false;
        ImGui::PushID(label);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

        float line_height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 button_size = {line_height + 3.0f, line_height};
        float width = ImGui::GetContentRegionAvail().x;
        float item_width = (width - static_cast<float>(count - 1) * ImGui::GetStyle().ItemSpacing.x) / static_cast<float>(count);

        const char* axis_names[] = {"X", "Y", "Z", "W"};
        const ImVec4 colors[] = {
            ImVec4(0.8f, 0.12f, 0.15f, 1.0f),  // Red
            ImVec4(0.18f, 0.65f, 0.18f, 1.0f),  // Green
            ImVec4(0.15f, 0.35f, 0.85f, 1.0f),  // Blue
            ImVec4(0.5f, 0.5f, 0.5f, 1.0f)      // Gray
        };

        for (int i = 0; i < count; ++i) {
            if (i > 0) {
                ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, colors[i]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(colors[i].x * 1.15f, colors[i].y * 1.15f, colors[i].z * 1.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(colors[i].x * 0.85f, colors[i].y * 0.85f, colors[i].z * 0.85f, 1.0f));

            if (ImGui::Button(axis_names[i], button_size)) {
                values[i] = config.reset;
                changed   = true;
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            ImGui::SetNextItemWidth(item_width - button_size.x);

            char id_buf[16];
            snprintf(id_buf, sizeof(id_buf), "##%s_%d", axis_names[i], i);
            if (ImGui::DragFloat(id_buf, &values[i], config.speed, config.min, config.max, "%.2f")) {
                changed = true;
            }
        }

        ImGui::PopStyleVar();
        ImGui::PopID();
        return changed;
    }
} // namespace

void lyra::ui::section(CString title, CString icon, ActionRef content)
{
    ImGui::PushID(title);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen 
                             | ImGuiTreeNodeFlags_Framed 
                             | ImGuiTreeNodeFlags_SpanAvailWidth 
                             | ImGuiTreeNodeFlags_AllowItemOverlap 
                             | ImGuiTreeNodeFlags_FramePadding;
    ImGui::Spacing();
    char header_buf[128];
    snprintf(header_buf, sizeof(header_buf), "%s %s", icon, title);
    if (ImGui::TreeNodeEx((void*)title, flags, "%s", header_buf)) {
        content();
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void lyra::ui::properties(ActionRef content)
{
    if (ImGui::BeginTable("##LyraProperties", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Values", ImGuiTableColumnFlags_WidthStretch);
        content();
        ImGui::EndTable();
    }
}

void lyra::ui::vec3(CString label, Vector3& value, const VecConfig& config)
{
    begin_property_row(label);
    draw_vec_row_internal(label, &value.x, 3, config);
}

void lyra::ui::vec3(CString label, Vector3& value, ActionRef on_change, const VecConfig& config)
{
    begin_property_row(label);
    if (draw_vec_row_internal(label, &value.x, 3, config)) {
        on_change();
    }
}

void lyra::ui::vec3(CString label, Vector3& value, ChangeRef<Vector3> on_change, const VecConfig& config)
{
    begin_property_row(label);
    if (draw_vec_row_internal(label, &value.x, 3, config)) {
        on_change(value);
    }
}

void lyra::ui::vec2(CString label, Vector2& value, const VecConfig& config)
{
    begin_property_row(label);
    draw_vec_row_internal(label, &value.x, 2, config);
}

void lyra::ui::vec2(CString label, Vector2& value, ActionRef on_change, const VecConfig& config)
{
    begin_property_row(label);
    if (draw_vec_row_internal(label, &value.x, 2, config)) {
        on_change();
    }
}

void lyra::ui::vec2(CString label, Vector2& value, ChangeRef<Vector2> on_change, const VecConfig& config)
{
    begin_property_row(label);
    if (draw_vec_row_internal(label, &value.x, 2, config)) {
        on_change(value);
    }
}

void lyra::ui::number(CString label, float& value, const ScalarConfig& config)
{
    begin_property_row(label);
    ImGui::DragFloat((String("##") + label).c_str(), &value, config.speed, config.min, config.max, "%.2f");
}

void lyra::ui::number(CString label, float& value, ActionRef on_change, const ScalarConfig& config)
{
    begin_property_row(label);
    if (ImGui::DragFloat((String("##") + label).c_str(), &value, config.speed, config.min, config.max, "%.2f")) {
        on_change();
    }
}

void lyra::ui::integer(CString label, int& value, int speed, int min, int max)
{
    begin_property_row(label);
    ImGui::DragInt((String("##") + label).c_str(), &value, static_cast<float>(speed), min, max);
}

void lyra::ui::integer(CString label, int& value, ActionRef on_change, int speed, int min, int max)
{
    begin_property_row(label);
    if (ImGui::DragInt((String("##") + label).c_str(), &value, static_cast<float>(speed), min, max)) {
        on_change();
    }
}

void lyra::ui::color(CString label, Vector3& rgb)
{
    begin_property_row(label);
    ImGui::ColorEdit3((String("##") + label).c_str(), &rgb.x);
}

void lyra::ui::color(CString label, Vector3& rgb, ActionRef on_change)
{
    begin_property_row(label);
    if (ImGui::ColorEdit3((String("##") + label).c_str(), &rgb.x)) {
        on_change();
    }
}

void lyra::ui::color(CString label, Vector4& rgba)
{
    begin_property_row(label);
    ImGui::ColorEdit4((String("##") + label).c_str(), &rgba.x);
}

void lyra::ui::color(CString label, Vector4& rgba, ActionRef on_change)
{
    begin_property_row(label);
    if (ImGui::ColorEdit4((String("##") + label).c_str(), &rgba.x)) {
        on_change();
    }
}

void lyra::ui::toggle(CString label, bool& value)
{
    begin_property_row(label);
    ImGui::Checkbox((String("##") + label).c_str(), &value);
}

void lyra::ui::toggle(CString label, bool& value, ChangeRef<bool> on_toggle)
{
    begin_property_row(label);
    if (ImGui::Checkbox((String("##") + label).c_str(), &value)) {
        on_toggle(value);
    }
}

void lyra::ui::text(CString label, char* buffer, size_t buffer_size)
{
    begin_property_row(label);
    ImGui::InputText((String("##") + label).c_str(), buffer, buffer_size);
}

void lyra::ui::text(CString label, char* buffer, size_t buffer_size, ActionRef on_commit)
{
    begin_property_row(label);
    if (ImGui::InputText((String("##") + label).c_str(), buffer, buffer_size, ImGuiInputTextFlags_EnterReturnsTrue)) {
        on_commit();
    }
}
