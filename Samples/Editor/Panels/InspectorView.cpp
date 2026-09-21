#include <Lyra/Common/Logger.h>
#include <Lyra/Scenes/World.h>
#include <Lyra/Scenes/Camera.h>
#include <Lyra/Scenes/Light.h>
#include <Lyra/Scenes/SceneNode.h>
#include <Lyra/Scenes/Transform.h>
#include <Lyra/UICore/UI.h>
#include <Lyra/UICore/UILayout.h>
#include <Lyra/UICore/UIControls.h>
#include <Lyra/UICore/UIProperty.h>
#include <Lyra/UICore/UIDock.h>
#include <Lyra/UICore/UIIcons.h>

#include "InspectorView.h"
#include "HierarchyView.h"

#define LYRA_INSPECTOR_WINDOW_NAME (LYRA_ICON_INSPECTOR " Inspector")

using namespace lyra;

namespace
{
    template <typename T, typename F>
    void draw_component(CString label, CString icon, World& world, SceneNode node, F func)
    {
        if (world.any_of<T>(node)) {
            auto& component = world.get_component<T>(node);
            ui::section(label, icon, [&]() {
                ui::properties([&]() {
                    func(component);
                });
            });
        }
    }
} // namespace

InspectorView::InspectorView()
{
    // do nothing
}

void InspectorView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &InspectorView::update>(*this);
}

void InspectorView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        ui::workspace::dock(LYRA_INSPECTOR_WINDOW_NAME, ui::Area::Right);
    });

    ui::panel(LYRA_INSPECTOR_WINDOW_NAME, [&]() {
        auto world     = blackboard.try_get<World*>();
        auto selection = blackboard.try_get<HierarchyView::Selection>();
        if (world && selection) {
            draw_inspector(**world, selection->node);
        }
    });
}

void InspectorView::draw_inspector(World& world, SceneNode node)
{
    if (node.entity == entt::null || !world.registry.valid(node.entity)) {
        ui::label("No node selected", ui::StatusRole::Muted);
        return;
    }

    // header section
    {
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        if (world.any_of<NodeName>(node)) {
            auto& name = world.get_component<NodeName>(node).name;
            strncpy_s(buffer, sizeof(buffer), name.c_str(), sizeof(buffer) - 1);
        } else {
            snprintf(buffer, sizeof(buffer), "Node %u", static_cast<uint32_t>(node.entity));
        }

        ui::row([&]() {
            ui::label(LYRA_ICON_SCENE);
            ui::text_field("name", buffer, sizeof(buffer), [&]() {
                world.set_name(node, buffer);
            });
        });
    }

    // components
    draw_component<TransformLocal>("Transform", LYRA_ICON_NODE, world, node, [&](TransformLocal& transform) {
        ui::vec3("Position", transform.position, [&]() {
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        });

        Vector3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
        ui::vec3("Rotation", euler, [&]() {
            transform.rotation = Quaternion(glm::radians(euler));
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        });

        ui::VecConfig scale_config;
        scale_config.speed = 0.1f;
        scale_config.min   = 0.0f;
        scale_config.max   = 0.0f;
        scale_config.reset = 1.0f;
        ui::vec3("Scale", transform.scale, [&]() {
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        }, scale_config);
    });

    draw_component<PerspectiveCamera>("Perspective Camera", LYRA_ICON_CAMERA, world, node, [&](PerspectiveCamera& camera) {
        ui::ScalarConfig fov_cfg;
        fov_cfg.speed = 0.1f;
        fov_cfg.min   = 1.0f;
        fov_cfg.max   = 179.0f;
        ui::number("FOV", camera.fov, fov_cfg);

        ui::ScalarConfig aspect_cfg;
        aspect_cfg.speed = 0.01f;
        aspect_cfg.min   = 0.1f;
        aspect_cfg.max   = 10.0f;
        ui::number("Aspect", camera.aspect, aspect_cfg);

        ui::ScalarConfig near_cfg;
        near_cfg.speed = 0.01f;
        near_cfg.min   = 0.001f;
        near_cfg.max   = 10.0f;
        ui::number("Near", camera.near_plane, near_cfg);

        ui::ScalarConfig far_cfg;
        far_cfg.speed = 1.0f;
        far_cfg.min   = 10.0f;
        far_cfg.max   = 10000.0f;
        ui::number("Far", camera.far_plane, far_cfg);
    });

    draw_component<OrthographicCamera>("Orthographic Camera", LYRA_ICON_CAMERA, world, node, [&](OrthographicCamera& camera) {
        ui::ScalarConfig size_cfg;
        size_cfg.speed = 0.1f;
        size_cfg.min   = 0.1f;
        size_cfg.max   = 1000.0f;
        ui::number("Size", camera.size, size_cfg);

        ui::ScalarConfig aspect_cfg;
        aspect_cfg.speed = 0.01f;
        aspect_cfg.min   = 0.1f;
        aspect_cfg.max   = 10.0f;
        ui::number("Aspect", camera.aspect, aspect_cfg);

        ui::ScalarConfig near_cfg;
        near_cfg.speed = 0.01f;
        near_cfg.min   = -1000.0f;
        near_cfg.max   = 1000.0f;
        ui::number("Near", camera.near_plane, near_cfg);

        ui::ScalarConfig far_cfg;
        far_cfg.speed = 0.01f;
        far_cfg.min   = -1000.0f;
        far_cfg.max   = 1000.0f;
        ui::number("Far", camera.far_plane, far_cfg);
    });

    draw_component<PointLight>("Point Light", LYRA_ICON_NODE, world, node, [&](PointLight& light) {
        ui::vec3("Position", light.position);
        ui::color("Color", light.color);

        ui::ScalarConfig intensity_cfg;
        intensity_cfg.speed = 1.0f;
        intensity_cfg.min   = 0.0f;
        intensity_cfg.max   = 100000.0f;
        ui::number("Intensity", light.intensity, intensity_cfg);

        ui::ScalarConfig falloff_cfg;
        falloff_cfg.speed = 0.1f;
        falloff_cfg.min   = 0.0f;
        falloff_cfg.max   = 100.0f;
        ui::number("Falloff", light.falloff, falloff_cfg);
    });

    draw_component<SpotLight>("Spot Light", LYRA_ICON_NODE, world, node, [&](SpotLight& light) {
        ui::vec3("Position", light.position);
        ui::vec3("Direction", light.direction);
        ui::color("Color", light.color);

        ui::ScalarConfig intensity_cfg;
        intensity_cfg.speed = 1.0f;
        intensity_cfg.min   = 0.0f;
        intensity_cfg.max   = 100000.0f;
        ui::number("Intensity", light.intensity, intensity_cfg);

        ui::ScalarConfig falloff_cfg;
        falloff_cfg.speed = 0.1f;
        falloff_cfg.min   = 0.0f;
        falloff_cfg.max   = 100.0f;
        ui::number("Falloff", light.falloff, falloff_cfg);

        ui::ScalarConfig angle_in;
        angle_in.speed = 0.1f;
        angle_in.min   = 0.0f;
        angle_in.max   = 90.0f;
        ui::number("Inner Angle", light.angle.x, angle_in);

        ui::ScalarConfig angle_out;
        angle_out.speed = 0.1f;
        angle_out.min   = 0.0f;
        angle_out.max   = 90.0f;
        ui::number("Outer Angle", light.angle.y, angle_out);
    });

    draw_component<DirectionalLight>("Directional Light", LYRA_ICON_NODE, world, node, [&](DirectionalLight& light) {
        ui::vec3("Direction", light.direction);
        ui::color("Color", light.color);

        ui::ScalarConfig intensity_cfg;
        intensity_cfg.speed = 1.0f;
        intensity_cfg.min   = 0.0f;
        intensity_cfg.max   = 100000.0f;
        ui::number("Intensity", light.intensity, intensity_cfg);
    });
}
