#include <Lyra/Utilities/Logger.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/Light.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/Transform.h>
#include <Lyra/Scripting/ScriptQuery.h>
#include <Lyra/Runtime/ScriptLayer.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UIProperty.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>

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

    void draw_related_system_item(ScriptLayer& scripting, const RelatedSystem& item)
    {
        const auto& sys        = *item.descriptor;
        ScriptID    id         = hash_script_name(sys.name);
        bool        is_enabled = scripting.is_script_enabled(id);

        ui::row([&]() {
            ui::checkbox(sys.name, is_enabled, [&](const bool& val) {
                scripting.set_script_enabled(id, val);
            });

            switch (sys.stage) {
                case AppEvent::UPDATE_PRE:
                    ui::badge("PRE_UPDATE", ui::StatusRole::Info);
                    break;
                case AppEvent::UPDATE:
                    ui::badge("UPDATE", ui::StatusRole::Success);
                    break;
                case AppEvent::UPDATE_POST:
                    ui::badge("POST_UPDATE", ui::StatusRole::Warning);
                    break;
                default:
                    break;
            }
        });

        if (!item.access_summary.empty()) {
            ui::label(item.access_summary.c_str(), ui::StatusRole::Muted);
        }
    }

    void draw_related_systems(ScriptLayer& scripting, World& world, SceneNode node)
    {
        auto matched_systems = find_related_systems(scripting, world, node);

        ui::section("Related Systems", LYRA_ICON_SYSTEM, [&]() {
            if (matched_systems.empty()) {
                ui::label("No active systems for this entity", ui::StatusRole::Muted);
                return;
            }

            for (size_t i = 0; i < matched_systems.size(); ++i) {
                if (i > 0) {
                    ui::separator();
                }
                draw_related_system_item(scripting, matched_systems[i]);
            }
        });
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

void InspectorView::update(AppContext& context)
{
    lyra::execute_once([&]() {
        ui::workspace::dock(LYRA_INSPECTOR_WINDOW_NAME, ui::Area::Right);
    });

    ui::panel(LYRA_INSPECTOR_WINDOW_NAME, [&]() {
        auto world     = context.toolboard.try_get<World*>();
        auto selection = context.blackboard.try_get<HierarchyView::Selection>();
        if (world && selection) {
            draw_inspector(context, *world, selection->node);
        }
    });
}

void InspectorView::draw_inspector(AppContext& context, World& world, SceneNode node)
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
    draw_component<TransformLocal>("Transform", LYRA_ICON_TRANSFORM, world, node, [&](TransformLocal& transform) {
        ui::vec3("Position", transform.position, [&]() {
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        });

        Vector3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
        ui::vec3("Rotation", euler, [&]() {
            transform.rotation = Quaternion(glm::radians(euler));
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        });

        ui::VectorConfig scale_config;
        scale_config.speed = 0.1f;
        scale_config.min   = 0.0f;
        scale_config.max   = 0.0f;
        scale_config.reset = 1.0f;
        ui::vec3("Scale", transform.scale, [&]() {
            transform.flags.set(TransformFlag::LOCAL_DIRTY);
        }, scale_config);
    });

    // reflected components
    if (auto scripting = context.toolboard.try_get<ScriptLayer*>()) {
        for (const auto& comp : scripting->get_components()) {
            if (StringView(comp.name) == "TransformLocal") {
                continue; // transform is drawn directly above with dirty flag handling
            }
            if (comp.has_component && comp.has_component(world, node)) {
                comp.draw_inspector(world, node);
            }
        }

        // related systems
        draw_related_systems(*scripting, world, node);
    }
}
