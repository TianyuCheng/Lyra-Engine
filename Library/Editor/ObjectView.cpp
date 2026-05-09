#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Scenes/World.h>
#include <Lyra/Scenes/Camera.h>
#include <Lyra/Scenes/Light.h>
#include <Lyra/Scenes/SceneNode.h>
#include <Lyra/Scenes/Transform.h>

#include <Lyra/Editor/Icons.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/ObjectView.h>
#include <Lyra/Editor/TreeView.h>

#define LYRA_INSPECTOR_WINDOW_NAME (LYRA_ICON_INSPECTOR " Inspector")

using namespace lyra;

namespace
{

    bool draw_vec3_control(const char* label, Vector3& values, float reset_value = 0.0f, float column_width = 150.0f)
    {
        bool changed = false;
        ImGui::PushID(label);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::TableNextColumn();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

        float  line_height = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
        ImVec2 button_size = {line_height + 3.0f, line_height};
        float  width       = ImGui::GetContentRegionAvail().x;

        float item_width = (width - 2.0f * GImGui->Style.ItemSpacing.x) / 3.0f;

        // X
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
        if (ImGui::Button("X", button_size)) {
            values.x = reset_value;
            changed  = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(item_width - button_size.x);
        if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
            changed = true;
        }

        // Y
        ImGui::SameLine(0, GImGui->Style.ItemSpacing.x);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
        if (ImGui::Button("Y", button_size)) {
            values.y = reset_value;
            changed  = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(item_width - button_size.x);
        if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
            changed = true;
        }

        // Z
        ImGui::SameLine(0, GImGui->Style.ItemSpacing.x);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
        if (ImGui::Button("Z", button_size)) {
            values.z = reset_value;
            changed  = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(item_width - button_size.x);
        if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
            changed = true;
        }

        ImGui::PopStyleVar();
        ImGui::PopID();

        return changed;
    }

    bool draw_property_row(const char* label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f, const char* format = "%.2f")
    {
        bool changed = false;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat((String("##") + label).c_str(), &value, speed, min, max, format)) {
            changed = true;
        }
        return changed;
    }

    bool draw_color_row(const char* label, Vector3& color)
    {
        bool changed = false;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit3((String("##") + label).c_str(), &color.x)) {
            changed = true;
        }
        return changed;
    }

    template <typename T, typename F>
    void draw_component(const char* label, const char* icon, World& world, SceneNode node, F func)
    {
        if (world.any_of<T>(node)) {
            auto& component = world.get_component<T>(node);
            ImGui::PushID(label);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;

            ImGui::Spacing();
            bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), flags, "%s %s", icon, label);

            if (open) {
                if (ImGui::BeginTable("##ComponentTable", 2, ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Values", ImGuiTableColumnFlags_WidthStretch);

                    func(component);

                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

} // namespace

ObjectView::ObjectView()
{
    // do nothing
}

void ObjectView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &ObjectView::update>(*this);
}

void ObjectView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_INSPECTOR_WINDOW_NAME, layout.right);
    });

    // imgui::disable_window_menu_button();
    ImGui::Begin(LYRA_INSPECTOR_WINDOW_NAME);
    {
        auto world     = blackboard.try_get<World*>();
        auto selection = blackboard.try_get<TreeView::Selection>();
        if (world && selection) {
            draw_inspector(**world, selection->node);
        }
    }
    ImGui::End();
}

void ObjectView::draw_inspector(World& world, SceneNode node)
{
    if (node.entity == entt::null || !world.registry.valid(node.entity)) {
        ImGui::TextDisabled("No node selected");
        return;
    }

    // Header section
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        if (world.any_of<NodeName>(node)) {
            auto& name = world.get_component<NodeName>(node).name;
            strncpy_s(buffer, sizeof(buffer), name.c_str(), sizeof(buffer) - 1);
        } else {
            sprintf(buffer, "Node %u", static_cast<uint32_t>(node.entity));
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(LYRA_ICON_SCENE);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            world.set_name(node, buffer);
        }

        ImGui::PopStyleVar();
    }

    // Components
    draw_component<TransformLocal>("Transform", LYRA_ICON_NODE, world, node, [&](TransformLocal& transform) {
        if (draw_vec3_control("Position", transform.position)) {
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        }

        Vector3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
        if (draw_vec3_control("Rotation", euler)) {
            transform.rotation = Quaternion(glm::radians(euler));
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        }

        if (draw_vec3_control("Scale", transform.scale, 1.0f)) {
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        }
    });

    draw_component<PerspectiveCamera>("Perspective Camera", LYRA_ICON_CAMERA, world, node, [&](PerspectiveCamera& camera) {
        draw_property_row("FOV", camera.fov, 0.1f, 1.0f, 179.0f);
        draw_property_row("Aspect", camera.aspect, 0.01f, 0.1f, 10.0f);
        draw_property_row("Near", camera.near_plane, 0.01f, 0.001f, 10.0f);
        draw_property_row("Far", camera.far_plane, 1.0f, 10.0f, 10000.0f);
    });

    draw_component<OrthographicCamera>("Orthographic Camera", LYRA_ICON_CAMERA, world, node, [&](OrthographicCamera& camera) {
        draw_property_row("Size", camera.size, 0.1f, 0.1f, 1000.0f);
        draw_property_row("Aspect", camera.aspect, 0.01f, 0.1f, 10.0f);
        draw_property_row("Near", camera.near_plane, 0.01f, -1000.0f, 1000.0f);
        draw_property_row("Far", camera.far_plane, 0.01f, -1000.0f, 1000.0f);
    });

    draw_component<PointLight>("Point Light", LYRA_ICON_NODE, world, node, [&](PointLight& light) {
        draw_vec3_control("Position", light.position);
        draw_color_row("Color", light.color);
        draw_property_row("Intensity", light.intensity, 1.0f, 0.0f, 100000.0f);
        draw_property_row("Falloff", light.falloff, 0.1f, 0.0f, 100.0f);
    });

    draw_component<SpotLight>("Spot Light", LYRA_ICON_NODE, world, node, [&](SpotLight& light) {
        draw_vec3_control("Position", light.position);
        draw_vec3_control("Direction", light.direction);
        draw_color_row("Color", light.color);
        draw_property_row("Intensity", light.intensity, 1.0f, 0.0f, 100000.0f);
        draw_property_row("Falloff", light.falloff, 0.1f, 0.0f, 100.0f);
        draw_property_row("Inner Angle", light.angle.x, 0.1f, 0.0f, 90.0f);
        draw_property_row("Outer Angle", light.angle.y, 0.1f, 0.0f, 90.0f);
    });

    draw_component<DirectionalLight>("Directional Light", LYRA_ICON_NODE, world, node, [&](DirectionalLight& light) {
        draw_vec3_control("Direction", light.direction);
        draw_color_row("Color", light.color);
        draw_property_row("Intensity", light.intensity, 1.0f, 0.0f, 100000.0f);
    });
}
