#pragma once

#ifndef LYRA_EDITOR_PANELS_SCENE_VIEW_H
#define LYRA_EDITOR_PANELS_SCENE_VIEW_H

// local imports
#include <Lyra/Runtime/Application.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/InputSystem/InputFilter.h>

#include "Common/ViewportCanvas.h"

namespace lyra
{
    struct SceneView : public ViewportInputProvider
    {
    public:
        using GizmoCaptureQuery = Function<bool() const>;

        explicit SceneView();

        void bind(Application& app);

        void update(AppContext& context);

        void render_default(GPUCommandBuffer command);

        auto get_backbuffer() -> Backbuffer
        {
            return canvas.get_backbuffer();
        }

        void set_gizmo_hover_query(GizmoCaptureQuery query) { gizmo_hover_query = std::move(query); }
        void set_gizmo_using_query(GizmoCaptureQuery query) { gizmo_using_query = std::move(query); }
        void clear_gizmo_capture_query()
        {
            gizmo_hover_query = nullptr;
            gizmo_using_query = nullptr;
        }

        bool is_gizmo_using() const { return gizmo_using_query ? gizmo_using_query() : false; }
        bool is_gizmo_hovered() const { return gizmo_hover_query ? gizmo_hover_query() : false; }
        bool is_gizmo_captured() const { return is_gizmo_hovered() || is_gizmo_using(); }

        bool is_canvas_hovered() const override { return canvas.is_hovered(); }
        bool is_overlay_interacting() const override { return is_gizmo_captured() || overlay_locked; }
        bool is_overlay_locked() const override { return overlay_locked; }
        bool is_mouse_nav_active() const override { return mouse_nav_active; }
        bool is_keyboard_nav_active() const override { return keyboard_nav_active; }

        bool is_viewport_active() const { return viewport_active; }
        bool is_viewport_dragging() const { return viewport_dragging; }
        bool is_camera_capturing() const { return camera_capturing; }

        auto&       get_canvas() { return canvas; }
        const auto& get_canvas() const { return canvas; }

    private:
        ViewportCanvas    canvas;
        GizmoCaptureQuery gizmo_hover_query   = nullptr;
        GizmoCaptureQuery gizmo_using_query   = nullptr;
        bool              overlay_locked      = false;
        bool              viewport_dragging   = false;
        bool              camera_capturing    = false;
        Vector2           captured_cursor_pos = Vector2(0.0f);
        bool              viewport_active     = false;
        bool              mouse_nav_active    = false;
        bool              keyboard_nav_active = false;
    };
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_SCENE_VIEW_H
