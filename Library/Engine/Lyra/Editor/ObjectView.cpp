#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/Transform.h>
#include <Lyra/Scene/SceneNode.h>
#include "Icons.h"
#include "Layout.h"
#include "ObjectView.h"
#include "TreeView.h"

#define LYRA_INSPECTOR_WINDOW_NAME (LYRA_ICON_INSPECTOR "Inspector")

using namespace lyra;

ObjectView::ObjectView()
{
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

    ImGui::Begin(LYRA_INSPECTOR_WINDOW_NAME);
    {
        World** world_ptr     = blackboard.try_get<World*>();
        auto*   selection_ptr = blackboard.try_get<TreeView::Selection>();

        if (world_ptr && selection_ptr) {
            draw_inspector(**world_ptr, selection_ptr->node);
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

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
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
}
