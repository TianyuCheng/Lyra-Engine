#pragma once

#ifndef LYRA_LYRA_EDITOR_SCENE_VIEW_H
#define LYRA_LYRA_EDITOR_SCENE_VIEW_H

// local imports
#include <Lyra/Editor/Canvas.h>
#include <Lyra/Player/Application.h>

namespace lyra
{
    struct SceneView
    {
    public:
        explicit SceneView();

        void bind(Application& app);

        void update(Blackboard& blackboard);

        void render_default(GPUCommandBuffer command);

        auto get_backbuffer() -> Backbuffer
        {
            return canvas.get_backbuffer();
        }

    private:
        Canvas canvas;
    };
} // namespace lyra

#endif // LYRA_LYRA_EDITOR_SCENE_VIEW_H
