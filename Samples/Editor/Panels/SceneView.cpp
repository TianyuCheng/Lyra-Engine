#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UIControls.h>

#include <Lyra/UISystem/Widgets/UIIcons.h>
#include <Lyra/UISystem/Widgets/UIDialog.h>
#include <Lyra/Scene/SceneManager.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/InputSystem/InputManager.h>

#include "SceneView.h"

#define LYRA_SCENE_WINDOW_NAME (LYRA_ICON_SCENE " Scene")

using namespace lyra;

SceneView::SceneView()
{
    // do nothing
}

void SceneView::bind(Application& app)
{
    // save asset manager into toolboard
    app.get_toolboard().add<SceneView*>(this);

    // bind layout manager events
    app.bind<AppEvent::UPDATE, &SceneView::update>(*this);

    // create canvas frames for GameView
    canvas.init(app.get_graphics_descriptor().frames);
}

void SceneView::update(AppContext& context)
{
    lyra::execute_once([&]() {
        ui::workspace::dock(LYRA_SCENE_WINDOW_NAME, ui::Area::Main);
    });

    auto clock = context.toolboard.get<Clock*>();

    ui::panel(LYRA_SCENE_WINDOW_NAME, [&]() {
        bool playing   = !clock->paused;
        auto scene_mgr = context.toolboard.try_get<SceneManager*>();

        ui::row([&]() {
            String scene_title = (scene_mgr && !scene_mgr->get_active_name().empty())
                                     ? scene_mgr->get_active_name()
                                     : "Untitled";
            if (scene_mgr && scene_mgr->is_dirty()) {
                scene_title += " *";
            }
            ui::label(LYRA_ICON_SCENE);
            ui::label(scene_title.c_str(), ui::StatusRole::Muted);
            ui::spacer();

            ui::icon_button(LYRA_ICON_PLAY, [&]() {
                clock->paused = false;
            }, "Play", playing ? ui::ButtonRole::Success : ui::ButtonRole::Standard);

            ui::icon_button(LYRA_ICON_PAUSE, [&]() {
                clock->paused = true;
            }, "Pause", !playing ? ui::ButtonRole::Warning : ui::ButtonRole::Standard);

            ui::icon_button(LYRA_ICON_RESTART, [&]() {
                clock->total_time = 0.0f;
                clock->paused     = false;
            }, "Restart", ui::ButtonRole::Standard);
            ui::spacer();
        });

        canvas.update(context);
        bool canvas_interacted = canvas.display();

        ui::drag_drop_target("LYRA_ASSET_MODEL", [&](const void* data, size_t) {
            auto path_cstr = static_cast<CString>(data);
            auto ams       = context.toolboard.try_get<AssetServer*>();
            if (ams && scene_mgr) {
                Path model_path(path_cstr);
                auto model_handle = ams->load_asset<ModelAsset>(path_cstr);
                if (!model_handle.valid()) {
                    model_handle = ams->load_asset<ModelAsset>(model_path.filename().generic_string().c_str());
                }
                if (!model_handle.valid() && ams->has_cooker_for(model_path)) {
                    auto [full_path, rel_path] = ams->resolve_asset_path(model_path);
                    auto    fut                = ams->import_asset(rel_path, false);
                    AssetID cooked_guid        = fut.get();
                    if (cooked_guid != 0) {
                        model_handle = ams->load_asset<ModelAsset>(cooked_guid);
                    }
                }
                bool spawned = false;
                if (model_handle.valid()) {
                    SpawnParams params;
                    params.name = model_path.stem().string();
                    auto node   = scene_mgr->spawn(model_handle, params);
                    if (node.entity != entt::null) {
                        scene_mgr->set_dirty(true);
                        spawned = true;
                    }
                }
                if (!spawned) {
                    ui::dialog::alert("Spawn Failed", "Failed to load or spawn model asset:\n" + model_path.filename().string() + "\n\nSee console log for error details.", ui::StatusRole::Error);
                }
            }
        });

        ui::drag_drop_target("LYRA_ASSET_SCENE", [&](const void* data, size_t) {
            auto path_cstr = static_cast<CString>(data);
            auto scene_mgr = context.toolboard.try_get<SceneManager*>();
            if (scene_mgr) {
                if (scene_mgr->is_dirty()) {
                    if (!ui::dialog::confirm("Unsaved Changes", "The active scene has unsaved changes. Discard and open this scene?")) {
                        return;
                    }
                }
                auto res = scene_mgr->load(Path(path_cstr), LoadMode::SINGLE);
                if (res == INVALID_SCENE_INSTANCE) {
                    ui::dialog::alert("Load Failed", "Failed to load scene:\n" + Path(path_cstr).filename().string() + "\n\nSee console log for error details.", ui::StatusRole::Error);
                }
            }
        });

        bool prev_active = viewport_active;
        bool modal_active = ui::is_modal_active();
        bool text_active  = ui::is_text_input_active();
        bool gizmo_active = is_gizmo_captured();
        bool canvas_hover = canvas.is_hovered();
        bool panel_focus  = ui::is_panel_focused();

        // mouse navigation is active when mouse is over canvas and not consumed by gizmo or modal
        mouse_nav_active = canvas_hover && !gizmo_active && !modal_active;

        // keyboard navigation is active when panel is focused or canvas hovered, without modals or text entry
        keyboard_nav_active = (panel_focus || canvas_hover || canvas_interacted) && !text_active && !modal_active;

        viewport_active = mouse_nav_active || keyboard_nav_active;

        if (auto input = context.try_tool<InputManager>()) {
            if (prev_active && !viewport_active) {
                input->reset();
            }
        }
    });
}

void SceneView::render_default(GPUCommandBuffer command)
{
    auto backbuffer = get_backbuffer();
    command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), shader_resource_state(GPUBarrierSync::ALL_SHADING)));
}
