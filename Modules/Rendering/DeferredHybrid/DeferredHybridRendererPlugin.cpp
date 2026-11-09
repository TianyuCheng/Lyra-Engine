#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/Pass.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

struct DeferredHybridRendererInstance
{
    RendererDescriptor descriptor;
    RendererSettings   settings;
    RendererStats      stats;

    // device & pipeline handles
    GPUDevice          device;
    GPURenderPipeline  gbuffer_pipeline;
    GPURenderPipeline  lighting_pipeline;
    GPUPipelineLayout  gbuffer_layout;
    GPUPipelineLayout  lighting_layout;

    // G-Buffer render targets
    GPUTexture         albedo_metallic_tex;
    GPUTextureView     albedo_metallic_view;
    GPUTexture         normal_roughness_tex;
    GPUTextureView     normal_roughness_view;
    GPUTexture         material_info_tex;
    GPUTextureView     material_info_view;
    GPUTexture         depth_stencil_tex;
    GPUTextureView     depth_stencil_view;

    // ray tracing acceleration & buffers
    GPUTlas            scene_tlas;
    GPUBuffer          camera_buffer;
    GPUBuffer          light_storage_buffer;
};

static auto get_name() -> CString
{
    return "Deferred Hybrid Renderer";
}

static auto get_capabilities() -> RendererCaps
{
    RendererCaps caps{};
    caps.features = RendererFeature::RASTERIZATION |
                    RendererFeature::HARDWARE_RAYTRACING |
                    RendererFeature::POST_PROCESSING |
                    RendererFeature::DEBUG_VISUALIZATION;
    caps.supported_primitives = PrimitiveSupport::MESH |
                                PrimitiveSupport::BVH |
                                PrimitiveSupport::SDF;
    caps.max_directional_lights = 4;
    caps.max_local_lights       = 4096;
    return caps;
}

static bool create_renderer(RendererHandle& out_handle, const RendererDescriptor& descriptor)
{
    auto instance = std::make_unique<DeferredHybridRendererInstance>();
    instance->descriptor = descriptor;
    instance->device     = descriptor.device;

    // TODO: 1. Compile G-Buffer rasterization pipeline (sys://shaders/Deferred/GBuffer.slang)
    // TODO: 2. Compile deferred lighting & ray tracing composition shader (sys://shaders/Deferred/DeferredLighting.slang)
    // TODO: 3. Create pipeline layouts for G-Buffer pass and lighting pass
    // TODO: 4. Allocate camera and light storage buffers
    // TODO: 5. Allocate initial G-Buffer textures (albedo, normal, material_info, depth)

    out_handle = RendererHandle(instance.release());
    return true;
}

static void delete_renderer(RendererHandle handle)
{
    if (auto ptr = handle.as_type<DeferredHybridRendererInstance>()) {
        // TODO: clean up G-Buffer render targets, pipelines, and acceleration structures
        if (ptr->albedo_metallic_tex.valid()) ptr->albedo_metallic_tex.destroy();
        if (ptr->albedo_metallic_view.valid()) ptr->albedo_metallic_view.destroy();
        if (ptr->normal_roughness_tex.valid()) ptr->normal_roughness_tex.destroy();
        if (ptr->normal_roughness_view.valid()) ptr->normal_roughness_view.destroy();
        if (ptr->material_info_tex.valid()) ptr->material_info_tex.destroy();
        if (ptr->material_info_view.valid()) ptr->material_info_view.destroy();
        if (ptr->depth_stencil_tex.valid()) ptr->depth_stencil_tex.destroy();
        if (ptr->depth_stencil_view.valid()) ptr->depth_stencil_view.destroy();
        if (ptr->camera_buffer.valid()) ptr->camera_buffer.destroy();
        if (ptr->light_storage_buffer.valid()) ptr->light_storage_buffer.destroy();
        if (ptr->gbuffer_pipeline.valid()) ptr->gbuffer_pipeline.destroy();
        if (ptr->lighting_pipeline.valid()) ptr->lighting_pipeline.destroy();
        if (ptr->gbuffer_layout.valid()) ptr->gbuffer_layout.destroy();
        if (ptr->lighting_layout.valid()) ptr->lighting_layout.destroy();
        delete ptr;
    }
}

static void resize(RendererHandle handle, uint width, uint height)
{
    if (auto ptr = handle.as_type<DeferredHybridRendererInstance>()) {
        ptr->descriptor.initial_extent = {width, height};
        // TODO: recreate G-Buffer textures (albedo, normal, material_info, depth) matching new extent
    }
}

static void set_settings(RendererHandle handle, const RendererSettings& settings)
{
    if (auto ptr = handle.as_type<DeferredHybridRendererInstance>()) {
        ptr->settings = settings;
        // TODO: toggle ray-traced reflections, ambient occlusion, or soft shadow tracing
    }
}

static void get_settings(RendererHandle handle, RendererSettings& out_settings)
{
    if (auto ptr = handle.as_type<DeferredHybridRendererInstance>()) {
        out_settings = ptr->settings;
    }
}

static void get_statistics(RendererHandle handle, RendererStats& out_stats)
{
    if (auto ptr = handle.as_type<DeferredHybridRendererInstance>()) {
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
    auto renderer = handle.as_type<DeferredHybridRendererInstance>();
    if (!renderer) {
        return;
    }

    // TODO: 1. G-Buffer Rasterization Pass:
    //          - Transition G-Buffer textures to color attachment state
    //          - Rasterize primary scene mesh geometry writing albedo, normals, roughness, metallic, and material ID
    // TODO: 2. Scene TLAS Acceleration Structure Update:
    //          - Rebuild or refit TLAS using GPUCommandEncoder::build_tlases
    // TODO: 3. Hybrid Ray Tracing & Deferred Lighting Pass:
    //          - Transition G-Buffer textures to shader read
    //          - Trace secondary shadow rays for directional and local lights
    //          - Trace reflection rays for mirror/glossy surfaces against TLAS
    //          - Evaluate lighting composition for standard PBR and user custom material shading
    // TODO: 4. Post-processing & Tonemapping Pass:
    //          - Apply exposure, bloom, and tonemap resolve to backbuffer target

    renderer->stats.draw_calls         = static_cast<uint>(scene.meshes.size());
    renderer->stats.triangles_rendered = static_cast<uint>(scene.meshes.size() * 12);
}

static void draw_debug_panel(RendererHandle handle)
{
    // TODO: render ImGui inspector widgets:
    //  - G-Buffer channel visualizer (Albedo, Normal, Roughness, Metallic, Depth, Material IDs)
    //  - Ray tracing debug view (ray hit distance, bounce heatmap)
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
