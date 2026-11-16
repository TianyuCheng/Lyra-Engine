#pragma once

#ifndef LYRA_SAMPLES_EDITOR_RENDERER_H
#define LYRA_SAMPLES_EDITOR_RENDERER_H

#include <Lyra/Lyra.hpp>
#include <Lyra/Rendering/Passes/EditorGridPass.h>

namespace lyra
{
    class EditorRenderer
    {
    public:
        void bind(Application& app);
        void render(const GPUSurfaceTexture& swapchain, AppContext& context, GPUCommandBuffer command);

    private:
        void init(AppContext& context);
        void update(AppContext& context);
        void destroy(AppContext& context);

    private:
        passes::EditorGridPass grid_pass;
        FrameGraphAllocator    fg_allocator;

        GPUTexture     depth_texture;
        GPUTextureView depth_view;

        SceneNode camera_node;
    };
} // namespace lyra

#endif // LYRA_SAMPLES_EDITOR_RENDERER_H
