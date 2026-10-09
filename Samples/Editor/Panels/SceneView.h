#pragma once

#ifndef LYRA_EDITOR_PANELS_SCENE_VIEW_H
#define LYRA_EDITOR_PANELS_SCENE_VIEW_H

// local imports
#include "Common/ViewportCanvas.h"
#include <Lyra/Runtime/Application.h>

#include <Lyra/Utilities/Function.h>

namespace lyra
{
    struct SceneView
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

        void set_gizmo_capture_query(GizmoCaptureQuery query) { gizmo_query = std::move(query); }
        void clear_gizmo_capture_query() { gizmo_query = nullptr; }
        bool is_gizmo_captured() const { return gizmo_query ? gizmo_query() : false; }

        bool is_viewport_active() const { return viewport_active; }
        bool is_canvas_hovered() const { return canvas.is_hovered(); }
        bool is_mouse_nav_active() const { return mouse_nav_active; }
        bool is_keyboard_nav_active() const { return keyboard_nav_active; }

        auto&       get_canvas() { return canvas; }
        const auto& get_canvas() const { return canvas; }

    private:
        ViewportCanvas    canvas;
        GizmoCaptureQuery gizmo_query         = nullptr;
        bool              viewport_active     = false;
        bool              mouse_nav_active    = false;
        bool              keyboard_nav_active = false;
    };
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_SCENE_VIEW_H
