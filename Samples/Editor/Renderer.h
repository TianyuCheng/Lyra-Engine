#ifndef LYRA_SAMPLES_EDITOR_RENDERER_H
#define LYRA_SAMPLES_EDITOR_RENDERER_H

#include <Lyra/Lyra.hpp>
#include <Lyra/Rendering/Passes/EditorGridPass.h>

using namespace lyra;

struct SampleCubeRenderer
{
public:
    void bind(Application& app);
    void render(const Backbuffer& backbuffer, AppContext& context, GPUCommandBuffer command);

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

#endif // LYRA_SAMPLES_EDITOR_RENDERER_H
