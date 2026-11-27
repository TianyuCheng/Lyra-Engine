#include <cmath>
#include <limits>

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Geometry.h>
#include <Lyra/Utilities/Intersect.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Components/Transform.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/SceneManager.h>
#include <Lyra/Rendering/ProceduralMesh.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/InputSystem/InputManager.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>
#include <Lyra/UISystem/Widgets/UIDialog.h>
#include <Lyra/UISystem/Widgets/UIGizmo.h>

#include "SceneView.h"
#include "HierarchyView.h"

#define LYRA_SCENE_WINDOW_NAME (LYRA_ICON_SCENE " Scene")

using namespace lyra;

SceneView::SceneView()
{
    // do nothing
}

void SceneView::bind(Application& app)
{
    // save viewport provider and scene view into services
    app.get_services().add<ViewportInputProvider*>(this);
    app.get_services().add<SceneView*>(this);

    // register gizmo hover and usage queries to delegate to ui::gizmo
    set_gizmo_hover_query([]() {
        return ui::gizmo::is_over();
    });
    set_gizmo_using_query([]() {
        return ui::gizmo::is_using();
    });

    // bind layout manager events
    app.bind<AppEvent::UPDATE, &SceneView::update>(*this);

    // create canvas frames for GameView
    canvas.init(app.get_graphics_descriptor().frames);
}

void SceneView::render_toolbar(AppContext& context, bool playing)
{
    auto clock     = context.services.get<Clock*>();
    auto scene_mgr = context.services.try_get<SceneManager*>();

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

        // transform tool modes: Translate (W), Rotate (E), Scale (R)
        bool is_translate = (gizmo_operation == ui::gizmo::Operation::Translate);
        ui::toggle_button("T", is_translate, [&](bool val) {
            if (val) gizmo_operation = ui::gizmo::Operation::Translate;
        });

        bool is_rotate = (gizmo_operation == ui::gizmo::Operation::Rotate);
        ui::toggle_button("R", is_rotate, [&](bool val) {
            if (val) gizmo_operation = ui::gizmo::Operation::Rotate;
        });

        bool is_scale = (gizmo_operation == ui::gizmo::Operation::Scale);
        ui::toggle_button("S", is_scale, [&](bool val) {
            if (val) gizmo_operation = ui::gizmo::Operation::Scale;
        });

        ui::separator();

        // coordinate space mode: Local vs World
        bool is_local = (gizmo_mode == ui::gizmo::Mode::Local);
        ui::toggle_button(is_local ? "Local" : "World", is_local, [&](bool val) {
            gizmo_mode = val ? ui::gizmo::Mode::Local : ui::gizmo::Mode::World;
        });

        // snapping toggle
        ui::toggle_button("Snap", gizmo_snap.snap_enabled, [&](bool val) {
            gizmo_snap.snap_enabled = val;
        });

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
}

void SceneView::handle_shortcuts(AppContext& context)
{
    // shortcuts active when panel is focused or canvas hovered, and NOT in camera capture or text typing
    if (camera_capturing || ui::is_text_input_active()) {
        return;
    }
    if (!canvas.is_hovered() && !ui::is_panel_focused()) {
        return;
    }

    if (ui::is_key_pressed(KeyButton::W)) {
        gizmo_operation = ui::gizmo::Operation::Translate;
    } else if (ui::is_key_pressed(KeyButton::E)) {
        gizmo_operation = ui::gizmo::Operation::Rotate;
    } else if (ui::is_key_pressed(KeyButton::R)) {
        gizmo_operation = ui::gizmo::Operation::Scale;
    } else if (ui::is_key_pressed(KeyButton::Q)) {
        gizmo_mode = (gizmo_mode == ui::gizmo::Mode::Local) ? ui::gizmo::Mode::World : ui::gizmo::Mode::Local;
    }
}

void SceneView::handle_gizmo(AppContext& context)
{
    auto world     = context.services.try_get<World*>();
    auto selection = context.services.try_get<HierarchyView::Selection*>();
    auto scene_mgr = context.services.try_get<SceneManager*>();
    if (!world || !selection) {
        return;
    }

    ui::gizmo::begin_frame();

    if (selection->node.entity == entt::null || !world->registry.valid(selection->node.entity)) {
        return;
    }

    auto entity = selection->node.entity;
    if (!world->registry.any_of<TransformWorld>(entity) || !world->registry.any_of<TransformLocal>(entity)) {
        return;
    }

    auto cam_view = world->registry.view<Camera, TransformWorld>();
    if (cam_view.begin() == cam_view.end()) {
        return;
    }
    auto  cam_entity = *cam_view.begin();
    auto& cam_comp   = cam_view.get<Camera>(cam_entity);
    auto& cam_world  = cam_view.get<TransformWorld>(cam_entity);

    Vector2 screen_pos = canvas.get_screen_pos();
    Vector2 extent     = canvas.get_extent();
    if (extent.x <= 0.0f || extent.y <= 0.0f) {
        return;
    }

    ui::gizmo::set_drawlist(nullptr);
    ui::gizmo::set_rect(screen_pos, extent);
    ui::gizmo::set_orthographic(cam_comp.type == ProjectionType::ORTHOGRAPHIC);

    Matrix4x4 view_matrix = glm::inverse(cam_world.xform);
    Matrix4x4 proj_matrix = cam_comp.projection;

    auto&     world_transform = world->registry.get<TransformWorld>(entity);
    auto&     local_transform = world->registry.get<TransformLocal>(entity);
    Matrix4x4 world_xform     = world_transform.xform;

    const float* snap_ptr       = nullptr;
    float        snap_values[3] = {0.0f, 0.0f, 0.0f};
    if (gizmo_snap.snap_enabled) {
        if (gizmo_operation == ui::gizmo::Operation::Translate) {
            snap_values[0] = gizmo_snap.translation_snap.x;
            snap_values[1] = gizmo_snap.translation_snap.y;
            snap_values[2] = gizmo_snap.translation_snap.z;
            snap_ptr       = snap_values;
        } else if (gizmo_operation == ui::gizmo::Operation::Rotate) {
            snap_values[0] = gizmo_snap.rotation_snap;
            snap_ptr       = snap_values;
        } else if (gizmo_operation == ui::gizmo::Operation::Scale) {
            snap_values[0] = gizmo_snap.scale_snap;
            snap_ptr       = snap_values;
        }
    }

    Matrix4x4 delta_xform(1.0f);
    bool      manipulated = ui::gizmo::manipulate(
        view_matrix,
        proj_matrix,
        gizmo_operation,
        gizmo_mode,
        world_xform,
        &delta_xform,
        snap_ptr);

    if (manipulated && ui::gizmo::is_using()) {
        auto parent_comp = world->registry.try_get<Parent>(entity);
        if (parent_comp && parent_comp->node.entity != entt::null && world->registry.valid(parent_comp->node.entity)) {
            auto&     parent_world = world->registry.get<TransformWorld>(parent_comp->node);
            Matrix4x4 inv_parent   = glm::inverse(parent_world.xform);
            Matrix4x4 local_mat    = inv_parent * world_xform;
            decompose_transform(local_mat, local_transform.position, local_transform.rotation, local_transform.scale);
        } else {
            decompose_transform(world_xform, local_transform.position, local_transform.rotation, local_transform.scale);
        }

        world_transform.xform = world_xform;
        local_transform.flags.set(TransformFlag::LOCAL_DIRTY);

        if (scene_mgr) {
            scene_mgr->set_dirty(true);
        }
    }
}

void SceneView::handle_picking(AppContext& context)
{
    if (!canvas.is_hovered() || is_gizmo_captured() || viewport_dragging || camera_capturing) {
        return;
    }
    if (!ui::is_mouse_clicked(MouseButton::LEFT)) {
        return;
    }

    auto world     = context.services.try_get<World*>();
    auto selection = context.services.try_get<HierarchyView::Selection*>();
    auto ams       = context.services.try_get<AssetServer*>();
    if (!world || !selection) {
        return;
    }

    auto cam_view = world->registry.view<Camera, TransformWorld>();
    if (cam_view.begin() == cam_view.end()) {
        return;
    }
    auto  cam_entity = *cam_view.begin();
    auto& cam_comp   = cam_view.get<Camera>(cam_entity);
    auto& cam_world  = cam_view.get<TransformWorld>(cam_entity);

    Vector2 screen_pos = canvas.get_screen_pos();
    Vector2 extent     = canvas.get_extent();
    if (extent.x <= 0.0f || extent.y <= 0.0f) {
        return;
    }

    Vector2 mouse_pos = ui::get_mouse_pos();
    float   rel_x     = mouse_pos.x - screen_pos.x;
    float   rel_y     = mouse_pos.y - screen_pos.y;
    if (rel_x < 0.0f || rel_x > extent.x || rel_y < 0.0f || rel_y > extent.y) {
        return;
    }

    float ndc_x = (rel_x / extent.x) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (rel_y / extent.y) * 2.0f;

    Matrix4x4 view_matrix = glm::inverse(cam_world.xform);
    Matrix4x4 inv_vp      = glm::inverse(cam_comp.projection * view_matrix);

    Vector4 p_near_h = inv_vp * Vector4(ndc_x, ndc_y, 0.0f, 1.0f);
    if (std::abs(p_near_h.w) < 1e-6f) return;
    Vector3 p_near = Vector3(p_near_h) / p_near_h.w;

    Vector4 p_far_h = inv_vp * Vector4(ndc_x, ndc_y, 1.0f, 1.0f);
    if (std::abs(p_far_h.w) < 1e-6f) return;
    Vector3 p_far = Vector3(p_far_h) / p_far_h.w;

    Vector3 ray_origin = (cam_comp.type == ProjectionType::PERSPECTIVE) ? Vector3(cam_world.xform[3]) : p_near;
    Vector3 ray_dir    = glm::normalize(p_far - ray_origin);
    Ray3D   ray(ray_origin, ray_dir);

    float  closest_dist = std::numeric_limits<float>::max();
    Entity hit_entity   = entt::null;

    auto xform_view = world->registry.view<TransformWorld>();
    for (auto entity : xform_view) {
        if (entity == cam_entity) {
            continue;
        }

        auto& tw = xform_view.get<TransformWorld>(entity);

        Bounds3D local_box;
        bool     has_bounds = false;

        if (world->registry.any_of<Mesh>(entity)) {
            auto&            mesh_comp = world->registry.get<Mesh>(entity);
            const MeshAsset* asset     = nullptr;
            if (const auto proc = find_procedural_mesh(mesh_comp.mesh.guid)) {
                asset = proc;
            } else if (ams) {
                asset = ams->get_asset(mesh_comp.mesh);
                if (!asset && mesh_comp.mesh.valid()) {
                    (void)ams->load_asset<MeshAsset>(mesh_comp.mesh.guid);
                    asset = ams->get_asset(mesh_comp.mesh);
                }
            }

            if (asset) {
                local_box       = Bounds3D(asset->min_bounds, asset->max_bounds);
                Vector3 extents = local_box.max - local_box.min;
                if (extents.x > 1e-4f || extents.y > 1e-4f || extents.z > 1e-4f) {
                    has_bounds = true;
                }
            }
            if (!has_bounds) {
                local_box  = Bounds3D(Vector3(-0.5f), Vector3(0.5f));
                has_bounds = true;
            }
        } else if (world->registry.any_of<DirectionalLight, PointLight, SpotLight, Camera>(entity)) {
            local_box  = Bounds3D(Vector3(-0.3f), Vector3(0.3f));
            has_bounds = true;
        }

        if (!has_bounds) {
            continue;
        }

        Matrix4x4 inv_model       = glm::inverse(tw.xform);
        Vector3   local_origin    = Vector3(inv_model * Vector4(ray.origin, 1.0f));
        Vector3   local_direction = Vector3(inv_model * Vector4(ray.direction, 0.0f));

        Ray3D local_ray(local_origin, local_direction);
        float t_min = 0.0f, t_max = 0.0f;
        if (intersects(local_ray, local_box, t_min, t_max)) {
            float   t          = (t_min >= 0.0f) ? t_min : t_max;
            Vector3 local_hit  = local_origin + t * local_direction;
            Vector3 world_hit  = Vector3(tw.xform * Vector4(local_hit, 1.0f));
            float   world_dist = glm::dot(world_hit - ray.origin, ray.direction);
            if (world_dist >= 0.0f && world_dist < closest_dist) {
                closest_dist = world_dist;
                hit_entity   = entity;
            }
        }
    }

    if (hit_entity != entt::null) {
        selection->node = SceneNode(hit_entity);
    } else {
        selection->node = SceneNode{};
    }
}

void SceneView::update(AppContext& context)
{
    ui::workspace::dock_default(LYRA_SCENE_WINDOW_NAME, ui::Area::Main);

    auto clock = context.services.get<Clock*>();

    ui::panel(LYRA_SCENE_WINDOW_NAME, [&]() {
        bool playing   = !clock->paused;
        auto scene_mgr = context.services.try_get<SceneManager*>();

        render_toolbar(context, playing);
        handle_shortcuts(context);

        canvas.update(context);
        canvas.display();

        // gizmo rendering & interaction
        handle_gizmo(context);

        // raycasting & object picking
        handle_picking(context);

        ui::drag_drop_target("LYRA_ASSET_MODEL", [&](const void* data, usize) {
            auto path_cstr = static_cast<CString>(data);
            auto ams       = context.services.try_get<AssetServer*>();
            if (ams && scene_mgr) {
                Path model_path(path_cstr);
                auto model_handle = ams->load_asset<ModelAsset>(path_cstr);
                if (!model_handle) {
                    model_handle = ams->load_asset<ModelAsset>(model_path.filename().generic_string().c_str());
                }
                if (!model_handle && ams->has_cooker_for(model_path)) {
                    auto [full_path, rel_path] = ams->resolve_asset_path(model_path);
                    auto    fut                = ams->import_asset(rel_path, false);
                    auto    cooked_guid        = fut.get();
                    if (cooked_guid && *cooked_guid != 0) {
                        model_handle = ams->load_asset<ModelAsset>(*cooked_guid);
                    }
                }
                bool spawned = false;
                if (model_handle && model_handle->valid()) {
                    SpawnParams params;
                    params.name = model_path.stem().string();
                    auto node   = scene_mgr->spawn(*model_handle, params);
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
            auto scene_mgr = context.services.try_get<SceneManager*>();
            if (scene_mgr) {
                if (scene_mgr->is_dirty()) {
                    if (!ui::dialog::confirm("Unsaved Changes", "The active scene has unsaved changes. Discard and open this scene?")) {
                        return;
                    }
                }
                auto res = scene_mgr->load(Path(path_cstr), LoadMode::SINGLE);
                if (!res) {
                    ui::dialog::alert("Load Failed", "Failed to load scene:\n" + Path(path_cstr).filename().string() + "\n\n" + res.error(), ui::StatusRole::Error);
                }
            }
        });

        bool canvas_hover = canvas.is_hovered();
        auto window       = context.try_service<Window>();

        // query mouse button state for active tool drag lock and camera look capture
        bool lmb_down = window ? window->get_input_state().is_mouse_down(MouseButton::LEFT) : false;
        bool rmb_down = window ? window->get_input_state().is_mouse_down(MouseButton::RIGHT) : false;
        bool mmb_down = window ? window->get_input_state().is_mouse_down(MouseButton::MIDDLE) : false;

        // nav_mouse_down only for navigation buttons (LMB is reserved for selection and gizmos)
        bool nav_mouse_down = rmb_down || mmb_down;

        // active tool interaction lock:
        // lock initiates if gizmo reports active use, or if hovered on canvas and LMB is held
        if (is_gizmo_using() || (canvas_hover && is_gizmo_hovered() && lmb_down)) {
            overlay_locked = true;
        }
        if (!lmb_down) {
            overlay_locked = false;
        }

        bool overlay_active = is_overlay_interacting();

        // viewport mouse drag capture:
        // retains mouse navigation across the desktop until navigation mouse buttons are released
        if (!overlay_active && canvas_hover && nav_mouse_down) {
            viewport_dragging = true;
        }
        if (!nav_mouse_down) {
            viewport_dragging = false;
        }

        // viewport camera look capture & cursor locking:
        // begins when RMB is pressed over canvas and not interacting with overlay tools
        if (window) {
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

        // keyboard navigation is active only during active camera look capture or when playing
        keyboard_nav_active = camera_capturing || playing;

        viewport_active = mouse_nav_active || keyboard_nav_active;
    });
}

void SceneView::render_default(GPUCommandBuffer command)
{
    auto backbuffer = get_backbuffer();
    command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
}
