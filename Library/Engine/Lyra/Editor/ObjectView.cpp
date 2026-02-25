#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/Transform.h>

#include "Icons.h"
#include "Layout.h"
#include "ObjectView.h"
#include "TreeView.h"

#define LYRA_INSPECTOR_WINDOW_NAME (LYRA_ICON_INSPECTOR "Inspector")

using namespace lyra;

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
        ImGui::Text("No node selected");
        return;
    }

    if (world.any_of<NodeName>(node)) {
        auto& name = world.get_component<NodeName>(node).name;
        ImGui::Text("Name: %s", name.c_str());
    } else {
        ImGui::Text("Node %u", static_cast<uint32_t>(node.entity));
    }

    ImGui::Separator();

    if (world.any_of<TransformLocal>(node)) {
        auto& transform = world.get_component<TransformLocal>(node);

        if (ImGui::CollapsingHeader(LYRA_ICON_NODE "Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            // translation
            if (ImGui::DragFloat3("Translation", &transform.position.x, 0.1f)) {
                transform.flags.set(TransformFlag::LOCAL_DIRTY);
            }

            // rotation
            Vector3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
            if (ImGui::DragFloat3("Rotation", &euler.x, 0.1f)) {
                transform.rotation = Quaternion(glm::radians(euler));
                transform.flags.set(TransformFlag::LOCAL_DIRTY);
            }

            // scale
            if (ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f)) {
                transform.flags.set(TransformFlag::LOCAL_DIRTY);
            }
        }
    }

    if (world.any_of<PerspectiveCamera>(node)) {
        auto& camera = world.get_component<PerspectiveCamera>(node);

        if (ImGui::CollapsingHeader(LYRA_ICON_CAMERA "Perspective", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("FOV", &camera.fov, 0.1f, 1.0f, 179.0f);
            ImGui::DragFloat("Aspect", &camera.aspect, 0.01f, 0.1f, 10.0f);
            ImGui::DragFloat("Near", &camera.near_plane, 0.01f, 0.001f, 10.0f);
            ImGui::DragFloat("Far", &camera.far_plane, 1.0f, 10.0f, 10000.0f);
        }
    }

    if (world.any_of<OrthographicCamera>(node)) {
        auto& camera = world.get_component<OrthographicCamera>(node);

        if (ImGui::CollapsingHeader(LYRA_ICON_CAMERA "Orthographic", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Size", &camera.size, 0.1f, 0.1f, 1000.0f);
            ImGui::DragFloat("Aspect", &camera.aspect, 0.01f, 0.1f, 10.0f);
            ImGui::DragFloat("Near", &camera.near_plane, 0.01f, -1000.0f, 1000.0f);
            ImGui::DragFloat("Far", &camera.far_plane, 0.01f, -1000.0f, 1000.0f);
        }
    }
}
