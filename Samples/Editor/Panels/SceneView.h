#pragma once

#ifndef LYRA_EDITOR_PANELS_SCENE_VIEW_H
#define LYRA_EDITOR_PANELS_SCENE_VIEW_H

// local imports
#include "Common/ViewportCanvas.h"
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    struct SceneView
    {
    public:
        explicit SceneView();

        void bind(Application& app);

        void update(AppContext& context);

        void render_default(GPUCommandBuffer command);

        auto get_backbuffer() -> Backbuffer
        {
            return canvas.get_backbuffer();
        }

        bool is_viewport_active() const { return viewport_active; }

    private:
        ViewportCanvas canvas;
        bool           viewport_active = false;
    };
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_SCENE_VIEW_H
