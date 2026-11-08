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
    // save viewport provider and scene view into toolboard
    app.get_toolboard().add<ViewportInputProvider*>(this);
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
            },
                "Play", playing ? ui::ButtonRole::Success : ui::ButtonRole::Standard);

            ui::icon_button(LYRA_ICON_PAUSE, [&]() {
                clock->paused = true;
            },
                "Pause", !playing ? ui::ButtonRole::Warning : ui::ButtonRole::Standard);

            ui::icon_button(LYRA_ICON_RESTART, [&]() {
                clock->total_time = 0.0f;
                clock->paused     = false;
            },
                "Restart", ui::ButtonRole::Standard);
            ui::spacer();
        });

        canvas.update(context);
        bool canvas_interacted = canvas.display();

        ui::drag_drop_target("LYRA_ASSET_MODEL", [&](const void* data, usize) {
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

        ui::drag_drop_target("LYRA_ASSET_SCENE", [&](const void* data, usize) {
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

        bool canvas_hover = canvas.is_hovered();
        bool panel_focus  = ui::is_panel_focused();

        // query mouse button state for active tool drag lock and camera look capture
        bool lmb_down = false;
        bool rmb_down = false;
        bool mmb_down = false;
        if (auto window = context.try_tool<Window>()) {
            lmb_down = window->get_input_state().is_mouse_down(MouseButton::LEFT);
            rmb_down = window->get_input_state().is_mouse_down(MouseButton::RIGHT);
            mmb_down = window->get_input_state().is_mouse_down(MouseButton::MIDDLE);
        }
        bool any_mouse_down = lmb_down || rmb_down || mmb_down;

        // active tool interaction lock:
        // lock initiates if gizmo reports active use, or if hovered on canvas and LMB is held
        // (suppressed if viewport drag has already claimed the mouse)
        if (is_gizmo_using() || (!viewport_dragging && canvas_hover && is_gizmo_hovered() && lmb_down)) {
            overlay_locked = true;
        }
        if (!lmb_down) {
            overlay_locked = false;
        }

        bool overlay_active = is_overlay_interacting();

        // viewport mouse drag capture:
        // retains mouse navigation across the desktop until all mouse buttons are released
        if (!overlay_active && canvas_hover && any_mouse_down) {
            viewport_dragging = true;
        }
        if (!any_mouse_down) {
            viewport_dragging = false;
        }

        // viewport camera look capture & cursor locking:
        // begins when RMB is pressed over canvas and not interacting with overlay tools
        if (auto window = context.try_tool<Window>()) {
            if (!camera_capturing) {
                if (canvas_hover && !overlay_active && rmb_down) {
                    camera_capturing    = true;
                    captured_cursor_pos = window->get_cursor_pos();
                    window->set_cursor_mode(CursorMode::LOCKED);
                }
            } else {
                // while capturing, keep camera look active until RMB is released
                if (!rmb_down) {
                    camera_capturing = false;
                    window->set_cursor_mode(CursorMode::NORMAL);
                    window->set_cursor_pos(captured_cursor_pos.x, captured_cursor_pos.y);
                }
            }
        }

        // mouse navigation is active when mouse is over canvas and not consumed by overlay/gizmo,
        // or during active viewport drag / camera look capture
        mouse_nav_active = (canvas_hover && !overlay_active) || viewport_dragging || camera_capturing;

        // keyboard navigation is active when panel is focused, canvas hovered, or during active drag / capture
        keyboard_nav_active = (panel_focus || canvas_hover || canvas_interacted || viewport_dragging || camera_capturing);

        viewport_active = mouse_nav_active || keyboard_nav_active;
    });
}

void SceneView::render_default(GPUCommandBuffer command)
{
    auto backbuffer = get_backbuffer();
    command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
}
