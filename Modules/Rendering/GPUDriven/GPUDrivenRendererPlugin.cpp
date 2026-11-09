#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/Pass.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

struct GPUDrivenRendererInstance
{
    RendererDescriptor descriptor;
    RendererSettings   settings;
    RendererStats      stats;

    // GPU-driven pipeline state
    GPUDevice           device;
    GPUComputePipeline  cull_pipeline;
    GPURenderPipeline   meshlet_pipeline;
    GPUPipelineLayout   pipeline_layout;

    // GPU buffers
    GPUBuffer           instance_buffer;
    GPUBuffer           indirect_draw_buffer;
    GPUBuffer           camera_uniform_buffer;

    // Hi-Z occlusion pyramid
    GPUTexture          hiz_texture;
    GPUTextureView      hiz_view;
};

static auto get_name() -> CString
{
    return "GPU Driven Renderer";
}

static auto get_capabilities() -> RendererCaps
{
    RendererCaps caps{};
    caps.features = RendererFeature::RASTERIZATION |
                    RendererFeature::COMPUTE_CULLING |
                    RendererFeature::MESH_SHADERS |
                    RendererFeature::POST_PROCESSING |
                    RendererFeature::DEBUG_VISUALIZATION;
    caps.supported_primitives = PrimitiveSupport::MESH |
                                PrimitiveSupport::MESHLET |
                                PrimitiveSupport::SDF;
    caps.max_directional_lights = 4;
    caps.max_local_lights       = 4096;
    return caps;
}

static bool create_renderer(RendererHandle& out_handle, const RendererDescriptor& descriptor)
{
    auto instance = std::make_unique<GPUDrivenRendererInstance>();
    instance->descriptor = descriptor;
    instance->device     = descriptor.device;

    // TODO: 1. Compile GPU compute culling shader (sys://shaders/GPUDriven/Culling.slang)
    // TODO: 2. Compile meshlet task and mesh shader rasterization pipeline
    // TODO: 3. Allocate persistent GPU instance buffer and indirect draw command buffer
    // TODO: 4. Allocate camera uniform buffer and bindless descriptor heap

    out_handle = RendererHandle(instance.release());
    return true;
}

static void delete_renderer(RendererHandle handle)
{
    if (auto ptr = handle.as_type<GPUDrivenRendererInstance>()) {
        // TODO: clean up allocated GPU buffers, pipelines, and Hi-Z textures
        if (ptr->instance_buffer.valid()) ptr->instance_buffer.destroy();
        if (ptr->indirect_draw_buffer.valid()) ptr->indirect_draw_buffer.destroy();
        if (ptr->camera_uniform_buffer.valid()) ptr->camera_uniform_buffer.destroy();
        if (ptr->hiz_texture.valid()) ptr->hiz_texture.destroy();
        if (ptr->hiz_view.valid()) ptr->hiz_view.destroy();
        if (ptr->meshlet_pipeline.valid()) ptr->meshlet_pipeline.destroy();
        if (ptr->cull_pipeline.valid()) ptr->cull_pipeline.destroy();
        if (ptr->pipeline_layout.valid()) ptr->pipeline_layout.destroy();
        delete ptr;
    }
}

static void resize(RendererHandle handle, uint width, uint height)
{
    if (auto ptr = handle.as_type<GPUDrivenRendererInstance>()) {
        ptr->descriptor.initial_extent = {width, height};
        // TODO: reallocate Hi-Z depth pyramid matching the new viewport extent
    }
}

static void set_settings(RendererHandle handle, const RendererSettings& settings)
{
    if (auto ptr = handle.as_type<GPUDrivenRendererInstance>()) {
        ptr->settings = settings;
        // TODO: update runtime toggles (e.g. enable/disable occlusion culling, wireframe mode)
    }
}

static void get_settings(RendererHandle handle, RendererSettings& out_settings)
{
    if (auto ptr = handle.as_type<GPUDrivenRendererInstance>()) {
        out_settings = ptr->settings;
    }
}

static void get_statistics(RendererHandle handle, RendererStats& out_stats)
{
    if (auto ptr = handle.as_type<GPUDrivenRendererInstance>()) {
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
    auto renderer = handle.as_type<GPUDrivenRendererInstance>();
    if (!renderer) {
        return;
    }

    // TODO: 1. Upload camera view & projection uniforms to camera_uniform_buffer
    // TODO: 2. Upload per-instance transform, bounding sphere, and material IDs to instance_buffer
    // TODO: 3. Dispatch compute culling pass (frustum culling + Hi-Z two-phase occlusion culling)
    // TODO: 4. Reset and generate indirect draw arguments via compute shader
    // TODO: 5. Execute indirect draw calls or meshlet task/mesh shader dispatches
    // TODO: 6. Support user custom material shading by indexing bindless material property arrays
    // TODO: 7. Post-processing and tonemapping pass resolve to target backbuffer

    renderer->stats.meshlets_rendered  = static_cast<uint>(scene.meshlets.size());
    renderer->stats.triangles_rendered = static_cast<uint>(scene.meshes.size() * 12);
}

static void draw_debug_panel(RendererHandle handle)
{
    // TODO: render ImGui inspector widgets:
    //  - GPU occlusion culling efficiency / culled instance count
    //  - Meshlet cluster LOD distribution
    //  - Hi-Z depth pyramid visualizer
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
