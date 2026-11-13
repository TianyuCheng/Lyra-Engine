#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/Pass.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

struct PathTracerRendererInstance
{
    RendererDescriptor descriptor;
    RendererSettings   settings;
    RendererStats      stats;

    // device & ray tracing pipeline
    GPUDevice             device;
    GPURayTracingPipeline rt_pipeline;
    GPUPipelineLayout     rt_layout;

    // shader binding table & accumulation buffers
    GPUBuffer          sbt_buffer;
    GPUBuffer          camera_buffer;
    GPUTexture         accumulation_tex;
    GPUTextureView     accumulation_view;
    GPUComputePipeline denoiser_pipeline;

    // temporal accumulation state
    uint accumulated_frames = 0;
};

static auto get_name() -> CString
{
    return "PathTracer";
}

static auto get_capabilities() -> RendererCapabilities
{
    RendererCapabilities caps{};
    caps.features = RendererFeature::HARDWARE_RAYTRACING |
                    RendererFeature::ACCUMULATION |
                    RendererFeature::POST_PROCESSING |
                    RendererFeature::DEBUG_VISUALIZATION;
    caps.supported_primitives = PrimitiveSupport::BVH |
                                PrimitiveSupport::MESH;
    caps.max_directional_lights = 16;
    caps.max_local_lights       = 4096;
    caps.supports_hdr           = true;
    return caps;
}

static bool create_renderer(RendererHandle& out_handle, const RendererDescriptor& descriptor)
{
    auto instance        = std::make_unique<PathTracerRendererInstance>();
    instance->descriptor = descriptor;
    instance->device     = descriptor.device;

    // TODO: 1. Compile Path Tracing shader modules (sys://shaders/PathTracer/PathTracer.slang)
    //          - RayGen shader (primary camera rays + path integration loop)
    //          - ClosestHit shader (BSDF evaluation, PBR and custom material shaders)
    //          - Miss shader (environment map sampling)
    // TODO: 2. Create Ray Tracing Pipeline and construct Shader Binding Table (SBT)
    // TODO: 3. Allocate temporal accumulation texture (RGBA32FLOAT)
    // TODO: 4. Allocate camera uniform buffer and random seed buffers
    // TODO: 5. Initialize denoising compute pipeline (bilateral / SVGF filter)

    out_handle = RendererHandle(instance.release());
    return true;
}

static void delete_renderer(RendererHandle handle)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
        // TODO: clean up RT pipeline, SBT buffer, accumulation textures, denoiser
        if (ptr->accumulation_tex.valid()) ptr->accumulation_tex.destroy();
        if (ptr->accumulation_view.valid()) ptr->accumulation_view.destroy();
        if (ptr->sbt_buffer.valid()) ptr->sbt_buffer.destroy();
        if (ptr->camera_buffer.valid()) ptr->camera_buffer.destroy();
        if (ptr->rt_pipeline.valid()) ptr->rt_pipeline.destroy();
        if (ptr->rt_layout.valid()) ptr->rt_layout.destroy();
        if (ptr->denoiser_pipeline.valid()) ptr->denoiser_pipeline.destroy();
        delete ptr;
    }
}

static void resize(RendererHandle handle, uint width, uint height)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
        ptr->descriptor.initial_extent = {width, height};
        // TODO: recreate accumulation buffer matching new dimensions and reset sample counter
        ptr->accumulated_frames = 0;
    }
}

static void set_settings(RendererHandle handle, const RendererSettings& settings)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
        if (ptr->settings.max_ray_bounces != settings.max_ray_bounces) {
            ptr->accumulated_frames = 0; // reset accumulation on bounce change
        }
        ptr->settings = settings;
    }
}

static void get_settings(RendererHandle handle, RendererSettings& out_settings)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
        out_settings = ptr->settings;
    }
}

static void get_statistics(RendererHandle handle, RendererStats& out_stats)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
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
    auto renderer = handle.as_type<PathTracerRendererInstance>();
    if (!renderer) {
        return;
    }

    // TODO: 1. Build or refit scene Top-Level Acceleration Structure (TLAS) from primitives
    // TODO: 2. Upload camera view/projection uniforms, sub-pixel jitter, and accumulation counter
    // TODO: 3. Dispatch ray tracing workgroups via raygen shader
    //          - Monte Carlo path integration with multiple bounces
    //          - Next-event estimation (direct analytical light sampling)
    //          - Material BSDF evaluation: both standard PBR and user custom material callables
    // TODO: 4. Accumulate samples into accumulation_tex (temporal integration across stationary frames)
    // TODO: 5. Execute denoiser compute pass (spatial/temporal filter)
    // TODO: 6. Tonemap, gamma correct, and blit resolved radiance to target backbuffer

    renderer->accumulated_frames++;
    renderer->stats.rays_traced        = target.extent.width * target.extent.height * renderer->settings.samples_per_pixel;
    renderer->stats.triangles_rendered = static_cast<uint>(scene.meshes.size() * 12);
}

static void draw_debug_panel(RendererHandle handle)
{
    // TODO: render ImGui inspector widgets:
    //  - Samples per pixel & accumulated frame counter
    //  - Maximum bounce depth slider
    //  - Reset accumulation button
    //  - Denoiser enable/disable toggle
    //  - Heatmap visualizer for ray traversal depth
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
