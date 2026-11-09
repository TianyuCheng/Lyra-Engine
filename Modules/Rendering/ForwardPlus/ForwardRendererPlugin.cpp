#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/Pass.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

struct ForwardRendererInstance
{
    RendererDescriptor descriptor;
    RendererSettings   settings;
    RendererStats      stats;

    // device & compiler
    GPUDevice          device;
    Compiler           compiler;

    // depth buffer
    GPUTexture         depth_texture;
    GPUTextureView     depth_view;

    // camera buffer & heap
    GPUBuffer          camera_buffer;
    GPUBindGroupHeap   heap;
    GPUBindGroup       camera_bind_group;

    // pipelines
    GPURenderPipeline  grid_pipeline;
    GPUPipelineLayout  grid_layout;

    GPURenderPipeline  forward_pipeline;
    GPUPipelineLayout  forward_layout;

    // light & cluster storage buffers for Forward+
    GPUBuffer          light_buffer;
    GPUBuffer          cluster_grid_buffer;
    GPUComputePipeline light_cull_pipeline;
};

static auto get_name() -> CString
{
    return "Forward/ForwardPlus Renderer";
}

static auto get_capabilities() -> RendererCaps
{
    RendererCaps caps{};
    caps.features = RendererFeature::RASTERIZATION |
                    RendererFeature::POST_PROCESSING |
                    RendererFeature::DEBUG_VISUALIZATION;
    caps.supported_primitives = PrimitiveSupport::MESH |
                                PrimitiveSupport::SDF;
    caps.max_directional_lights = 4;
    caps.max_local_lights       = 2048;
    return caps;
}

static bool create_renderer(RendererHandle& out_handle, const RendererDescriptor& descriptor)
{
    auto instance = std::make_unique<ForwardRendererInstance>();
    instance->descriptor = descriptor;
    instance->device     = descriptor.device;
    instance->compiler   = descriptor.compiler;

    // TODO: 1. Compile editor infinite grid pipeline (sys://shaders/Editor/Grid.slang)
    // TODO: 2. Compile forward PBR and custom material shading pipeline (sys://shaders/Forward/Forward.slang)
    // TODO: 3. Compile clustered light culling compute shader for Forward+ clustered shading
    // TODO: 4. Allocate camera uniform buffer and bind group layout
    // TODO: 5. Allocate light buffer and cluster assignment grid buffer

    out_handle = RendererHandle(instance.release());
    return true;
}

static void delete_renderer(RendererHandle handle)
{
    if (auto ptr = handle.as_type<ForwardRendererInstance>()) {
        // TODO: destroy allocated GPU textures, buffers, and pipeline states
        if (ptr->depth_texture.valid()) ptr->depth_texture.destroy();
        if (ptr->depth_view.valid()) ptr->depth_view.destroy();
        if (ptr->camera_buffer.valid()) ptr->camera_buffer.destroy();
        if (ptr->light_buffer.valid()) ptr->light_buffer.destroy();
        if (ptr->cluster_grid_buffer.valid()) ptr->cluster_grid_buffer.destroy();
        if (ptr->grid_pipeline.valid()) ptr->grid_pipeline.destroy();
        if (ptr->grid_layout.valid()) ptr->grid_layout.destroy();
        if (ptr->forward_pipeline.valid()) ptr->forward_pipeline.destroy();
        if (ptr->forward_layout.valid()) ptr->forward_layout.destroy();
        if (ptr->light_cull_pipeline.valid()) ptr->light_cull_pipeline.destroy();
        delete ptr;
    }
}

static void resize(RendererHandle handle, uint width, uint height)
{
    if (auto ptr = handle.as_type<ForwardRendererInstance>()) {
        ptr->descriptor.initial_extent = {width, height};
        // TODO: recreate depth texture and depth view matching new extent
    }
}

static void set_settings(RendererHandle handle, const RendererSettings& settings)
{
    if (auto ptr = handle.as_type<ForwardRendererInstance>()) {
        ptr->settings = settings;
        // TODO: update runtime toggles (wireframe mode, debug visualization modes, exposure)
    }
}

static void get_settings(RendererHandle handle, RendererSettings& out_settings)
{
    if (auto ptr = handle.as_type<ForwardRendererInstance>()) {
        out_settings = ptr->settings;
    }
}

static void get_statistics(RendererHandle handle, RendererStats& out_stats)
{
    if (auto ptr = handle.as_type<ForwardRendererInstance>()) {
        out_stats = ptr->stats;
    }
}

static void render(
    RendererHandle          handle,
    const RenderView&       view,
    const RenderScene&      scene,
    const Backbuffer&       target,
    GPUCommandEncoderHandle command_encoder)
{
    auto renderer = handle.as_type<ForwardRendererInstance>();
    if (!renderer) {
        return;
    }

    // TODO: 1. Ensure depth texture matches target extent
    // TODO: 2. Upload camera view & projection uniforms to camera_buffer
    // TODO: 3. Forward+ Clustered Lighting Pass:
    //          - Upload analytical lights (directional, point, spot) to light_buffer
    //          - Dispatch compute shader to assign lights into 3D camera frustum cluster grid
    // TODO: 4. Optional Depth Prepass:
    //          - If depth prepass is enabled in settings, rasterize scene mesh geometry with color writes disabled
    // TODO: 5. Forward Shading Pass:
    //          - Render editor infinite grid if enabled
    //          - Render scene meshes: evaluate standard PBR shading or user custom material shading
    //            based on prim.shading_model (0 = PBR, 1 = Unlit, 2 = Toon, 3 = Subsurface, 4 = Custom)
    // TODO: 6. Tonemapping and post-processing resolve to target backbuffer

    renderer->stats.draw_calls         = static_cast<uint>(scene.meshes.size());
    renderer->stats.triangles_rendered = static_cast<uint>(scene.meshes.size() * 12);
}

static void draw_debug_panel(RendererHandle handle)
{
    // TODO: render ImGui inspector widgets:
    //  - Light cluster heatmap visualizer (lights per cluster)
    //  - Shading model distribution (PBR, Unlit, Toon, Custom)
    //  - Depth prepass enable/disable toggle
    //  - Wireframe mode toggle
}

LYRA_EXPORT auto prepare() -> void
{
}

LYRA_EXPORT auto cleanup() -> void
{
}

LYRA_EXPORT auto create() -> RendererAPI
{
    RendererAPI api{};
    api.get_name         = get_name;
    api.get_capabilities = get_capabilities;
    api.create_renderer  = create_renderer;
    api.delete_renderer  = delete_renderer;
    api.resize           = resize;
    api.set_settings     = set_settings;
    api.get_settings     = get_settings;
    api.get_statistics   = get_statistics;
    api.render           = render;
    api.draw_debug_panel = draw_debug_panel;
    return api;
}
