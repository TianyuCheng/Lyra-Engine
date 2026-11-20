#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/GBufferPass.h>
#include <Lyra/Rendering/Passes/ShadowMapPass.h>
#include <Lyra/Rendering/Passes/SkyboxPass.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

namespace
{
    struct CameraUniforms
    {
        Matrix4x4 proj;
        Matrix4x4 view;
        Matrix4x4 inv_proj;
        Matrix4x4 inv_view;
        Vector4   pos_and_intensity;
        Vector4   sun_dir_and_pad;
        Vector4   sun_color_and_pad;
        Matrix4x4 light_view_proj = Matrix4x4(1.0f);
    };
} // namespace

struct DeferredRendererInstance
{
    static constexpr uint FRAMES_IN_FLIGHT = 3;

    RendererDescriptor descriptor;
    RendererSettings   settings;
    RendererStats      stats;

    GPUDevice device;
    Compiler  compiler;

    // composed passes
    passes::GBufferPass   gbuffer_pass;
    passes::ShadowMapPass shadow_pass;
    passes::SkyboxPass    skybox_pass;

    Array<GPUBuffer, FRAMES_IN_FLIGHT> camera_buffers;
    uint                               frame_slot = 0;

    // lighting resolve pipeline handles
    GPUShaderModule                  vshader;
    GPUShaderModule                  fshader;
    GPUPipelineLayout                lighting_playout;
    GPURenderPipeline                lighting_pipeline;
    GPUBindGroupHeap                 heap;
    Vector<GPUBindGroupLayoutHandle> blayouts;

    GPUSampler     shadow_sampler;
    GPUTexture     default_shadow_map;
    GPUTextureView default_shadow_view;

    GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
    GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;

    bool initialized = false;

    bool init(
        GPUDevice        in_device,
        Compiler         in_compiler,
        GPUTextureFormat color_format = GPUTextureFormat::RGBA8UNORM,
        GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT)
    {
        if (initialized) {
            if (cached_color_format == color_format && cached_depth_format == depth_format) {
                return true;
            }
            shutdown();
        }

        device              = in_device;
        compiler            = in_compiler;
        cached_color_format = color_format;
        cached_depth_format = depth_format;

        if (!RHI::api() || !compiler.handle.valid()) {
            return false;
        }

        // 1. Initialize G-Buffer pass
        if (!gbuffer_pass.init(device, compiler, depth_format)) {
            return false;
        }

        // 2. Initialize shadow map pass
        shadow_pass.init(device, compiler);

        // 3. Initialize skybox pass
        skybox_pass.init(device, compiler, cached_color_format, cached_depth_format);

        // 4. Create camera buffers for deferred lighting
        for (uint i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            camera_buffers[i] = execute([&]() {
                auto bdesc               = GPUBufferDescriptor{};
                bdesc.label              = "deferred_lighting_camera_buffer";
                bdesc.size               = sizeof(CameraUniforms);
                bdesc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
                bdesc.mapped_at_creation = true;
                return device.create_buffer(bdesc);
            });
        }

        // 5. Compile DeferredLighting shader
        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "deferred_lighting";
            desc.path   = "sys://Shaders/Deferred/DeferredLighting.slang";
            desc.source = nullptr;
            return compiler.compile(desc);
        });

        if (!module) {
            return false;
        }

        auto reflection = compiler.reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        if (!reflection) {
            return false;
        }

        vshader = execute([&]() {
            auto code  = module->get_shader_blob("vsmain");
            auto desc  = GPUShaderModuleDescriptor{};
            desc.label = "deferred_lighting_vs";
            desc.data  = code->data;
            desc.size  = code->size;
            return device.create_shader_module(desc);
        });

        fshader = execute([&]() {
            auto code  = module->get_shader_blob("fsmain");
            auto desc  = GPUShaderModuleDescriptor{};
            desc.label = "deferred_lighting_fs";
            desc.data  = code->data;
            desc.size  = code->size;
            return device.create_shader_module(desc);
        });

        blayouts.clear();
        for (auto& bdesc : reflection->get_bind_group_layouts()) {
            blayouts.push_back(device.create_bind_group_layout(bdesc));
        }

        lighting_playout = execute([&]() {
            auto pdesc               = GPUPipelineLayoutDescriptor{};
            pdesc.bind_group_layouts = blayouts;
            pdesc.immediate_size     = reflection->get_immediate_size();
            return device.create_pipeline_layout(pdesc);
        });

        lighting_pipeline = execute([&]() {
            Vector<GPUColorTargetState> rstates(1);
            rstates[0].format       = cached_color_format;
            rstates[0].blend_enable = false;

            auto pdesc                         = GPURenderPipelineDescriptor{};
            pdesc.layout                       = lighting_playout;
            pdesc.primitive.cull_mode          = GPUCullMode::NONE;
            pdesc.primitive.topology           = GPUPrimitiveTopology::TRIANGLE_LIST;
            pdesc.primitive.front_face         = GPUFrontFace::CCW;
            pdesc.primitive.strip_index_format = GPUIndexFormat::UINT32;
            pdesc.multisample.count            = 1;
            pdesc.vertex.module                = vshader;
            pdesc.vertex.buffers               = {};
            pdesc.vertex.entry_point           = "vsmain";
            pdesc.fragment.module              = fshader;
            pdesc.fragment.targets             = rstates;
            pdesc.fragment.entry_point         = "fsmain";
            return device.create_render_pipeline(pdesc);
        });

        heap = execute([&]() {
            auto hdesc      = GPUBindGroupHeapDescriptor{};
            hdesc.page_size = 512;
            return device.create_bind_group_heap(hdesc);
        });

        shadow_sampler = execute([&]() {
            auto sdesc           = GPUSamplerDescriptor{};
            sdesc.label          = "deferred_shadow_sampler";
            sdesc.min_filter     = GPUFilterMode::LINEAR;
            sdesc.mag_filter     = GPUFilterMode::LINEAR;
            sdesc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
            sdesc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
            return device.create_sampler(sdesc);
        });

        default_shadow_map  = execute([&]() {
            auto sdesc   = GPUTextureDescriptor{};
            sdesc.label  = "deferred_default_shadow_map";
            sdesc.size   = {1, 1, 1};
            sdesc.format = GPUTextureFormat::DEPTH32FLOAT;
            sdesc.usage  = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::RENDER_ATTACHMENT;
            return device.create_texture(sdesc);
        });
        default_shadow_view = default_shadow_map.create_view();

        initialized = gbuffer_pass.valid() && lighting_pipeline.valid();
        return initialized;
    }

    void shutdown()
    {
        if (!initialized) {
            return;
        }

        device.wait();

        gbuffer_pass.destroy();
        shadow_pass.destroy();
        skybox_pass.destroy();

        for (auto& buf : camera_buffers) {
            if (buf.valid()) {
                buf.destroy();
            }
        }
        camera_buffers.fill({});

        if (shadow_sampler.valid()) shadow_sampler.destroy();
        if (default_shadow_view.valid()) default_shadow_view.destroy();
        if (default_shadow_map.valid()) default_shadow_map.destroy();

        if (heap.valid()) heap.destroy();
        if (lighting_pipeline.valid()) lighting_pipeline.destroy();
        if (lighting_playout.valid()) lighting_playout.destroy();
        if (fshader.valid()) fshader.destroy();
        if (vshader.valid()) vshader.destroy();
        blayouts.clear();

        initialized = false;
    }
};

static auto get_name() -> CString
{
    return "Deferred Renderer";
}

static auto get_capabilities() -> RendererCapabilities
{
    RendererCapabilities caps{};
    caps.features = RendererFeature::RASTERIZATION |
                    RendererFeature::HARDWARE_RAYTRACING |
                    RendererFeature::POST_PROCESSING |
                    RendererFeature::DEBUG_VISUALIZATION;
    caps.supported_primitives = PrimitiveSupport::MESH |
                                PrimitiveSupport::BVH;
    caps.max_directional_lights = 4;
    caps.max_local_lights       = 4096;
    return caps;
}

static bool create_renderer(RendererHandle& out_handle, const RendererDescriptor& descriptor)
{
    auto instance        = std::make_unique<DeferredRendererInstance>();
    instance->descriptor = descriptor;
    instance->device     = descriptor.device;
    instance->compiler   = descriptor.compiler;

    if (RHI::api() && descriptor.compiler.handle.valid()) {
        if (!instance->init(descriptor.device, descriptor.compiler, descriptor.color_format, descriptor.depth_format)) {
            return false;
        }
    }

    out_handle = RendererHandle(instance.release());
    return true;
}

static void delete_renderer(RendererHandle handle)
{
    if (auto ptr = handle.as_type<DeferredRendererInstance>()) {
        ptr->shutdown();
        delete ptr;
    }
}

static void resize(RendererHandle handle, uint width, uint height)
{
    if (auto ptr = handle.as_type<DeferredRendererInstance>()) {
        ptr->descriptor.initial_extent = {width, height};
    }
}

static void set_settings(RendererHandle handle, const RendererSettings& settings)
{
    if (auto ptr = handle.as_type<DeferredRendererInstance>()) {
        ptr->settings = settings;
    }
}

static void get_settings(RendererHandle handle, RendererSettings& out_settings)
{
    if (auto ptr = handle.as_type<DeferredRendererInstance>()) {
        out_settings = ptr->settings;
    }
}

static void get_statistics(RendererHandle handle, RendererStats& out_stats)
{
    if (auto ptr = handle.as_type<DeferredRendererInstance>()) {
        out_stats = ptr->stats;
    }
}

static FrameGraph::TextureHandle render(
    RendererHandle            handle,
    const RenderFrameContext& ctx)
{
    auto renderer = handle.as_type<DeferredRendererInstance>();
    if (!renderer) {
        return ctx.color_target;
    }

    if (!ctx.color_target.valid()) {
        return ctx.color_target;
    }

    GPUExtent2D extent = (ctx.render_extent.width > 0 && ctx.render_extent.height > 0)
                             ? ctx.render_extent
                             : ctx.display_extent;

    if (extent.width == 0 || extent.height == 0) {
        return ctx.color_target;
    }

    // lazy-init if created without active RHI or if formats changed
    if (!renderer->initialized && RHI::api()) {
        auto dev  = RHI::get_current_device();
        auto comp = renderer->compiler.handle.valid() ? renderer->compiler : (ctx.services ? *ctx.services->try_get<Compiler*>() : Compiler{});
        if (comp.handle.valid()) {
            renderer->init(dev, comp, ctx.color_format, ctx.depth_format);
        }
    }

    if (!renderer->initialized) {
        return ctx.color_target;
    }

    RenderView view    = ctx.view;
    view.render_extent = extent;

    // 0. Cascaded shadow mapping pass (if shadows enabled and directional lights exist)
    FrameGraphHandle<FrameGraphTexture> shadow_map_h{};
    if (renderer->settings.enable_shadows && !ctx.scene.directional_lights.empty() && renderer->shadow_pass.valid()) {
        auto s_data  = renderer->shadow_pass.add(ctx.builder, view, ctx.scene);
        shadow_map_h = s_data.shadow_map_array;
    }

    renderer->frame_slot = (renderer->frame_slot + 1) % DeferredRendererInstance::FRAMES_IN_FLIGHT;
    uint current_slot    = renderer->frame_slot;

    if (renderer->camera_buffers[current_slot].valid()) {
        auto mapped = renderer->camera_buffers[current_slot].get_mapped_range<CameraUniforms>();
        if (mapped.count > 0) {
            mapped.at(0).proj     = view.projection;
            mapped.at(0).view     = view.view;
            mapped.at(0).inv_proj = view.inv_projection;
            mapped.at(0).inv_view = view.inv_view;
            float   exposure_scale = (view.exposure_scale > 0.0f) ? view.exposure_scale : (1.0f / (1.2f * 8192.0f));
            Vector3 sun_dir        = glm::normalize(Vector3(0.5f, 1.0f, 0.3f));
            Vector3 sun_color      = Vector3(1.0f);
            float   sun_intensity  = 110000.0f * exposure_scale;

            if (!ctx.scene.directional_lights.empty()) {
                const auto& light = ctx.scene.directional_lights[0];
                if (glm::length2(light.direction) > 1e-4f) {
                    sun_dir = -glm::normalize(light.direction);
                }
                sun_color     = light.color;
                sun_intensity = std::max(light.intensity * exposure_scale, 0.01f);
            }

            mapped.at(0).pos_and_intensity = Vector4(view.camera_position, sun_intensity);
            mapped.at(0).sun_dir_and_pad   = Vector4(sun_dir, 0.0f);
            mapped.at(0).sun_color_and_pad = Vector4(sun_color, 0.0f);
            mapped.at(0).light_view_proj   = renderer->shadow_pass.get_light_view_proj();
        }
    }

    // 1. G-Buffer pass
    auto gbuffer = renderer->gbuffer_pass.add(ctx.builder, view, ctx.scene, extent);

    // 2. Deferred lighting composition pass
    struct DeferredLightingPassData
    {
        FrameGraphHandle<FrameGraphTexture> albedo;
        FrameGraphHandle<FrameGraphTexture> normal;
        FrameGraphHandle<FrameGraphTexture> material;
        FrameGraphHandle<FrameGraphTexture> depth;
        FrameGraphHandle<FrameGraphTexture> shadow_map;
        FrameGraphHandle<FrameGraphTexture> lit_output;
    };

    auto lighting_data = ctx.builder.add_pass<DeferredLightingPassData>(
        "DeferredLightingPass",
        [&](DeferredLightingPassData& data, FrameGraphPassBuilder& pass_builder) {
            data.albedo   = pass_builder.sample(gbuffer.albedo_metallic);
            data.normal   = pass_builder.sample(gbuffer.normal_roughness);
            data.material = pass_builder.sample(gbuffer.material_info);
            data.depth    = pass_builder.sample(gbuffer.depth_stencil);
            if (shadow_map_h.valid()) {
                data.shadow_map = pass_builder.sample(shadow_map_h);
            }
            data.lit_output = pass_builder.render(ctx.color_target);
        },
        [renderer, extent, current_slot](const DeferredLightingPassData& data, FrameGraphResources& resources, FrameGraphContext* fg_ctx) {
            if (!fg_ctx || !fg_ctx->cmdlist.handle.valid() || !renderer->lighting_pipeline.valid()) {
                return;
            }

            auto albedo_tex = resources.get<FrameGraphTexture>(data.albedo);
            auto normal_tex = resources.get<FrameGraphTexture>(data.normal);
            auto mat_tex    = resources.get<FrameGraphTexture>(data.material);
            auto depth_tex  = resources.get<FrameGraphTexture>(data.depth);
            auto lit_tex    = resources.get<FrameGraphTexture>(data.lit_output);

            if (!albedo_tex || !normal_tex || !mat_tex || !depth_tex || !lit_tex) {
                return;
            }

            GPUTextureViewHandle s_view = renderer->default_shadow_view.handle;
            if (data.shadow_map.valid()) {
                if (auto shadow_tex = resources.get<FrameGraphTexture>(data.shadow_map)) {
                    s_view = shadow_tex->view;
                }
            }

            Array<GPUBindGroupEntry, 7> entries{};
            entries[0].type    = GPUResourceType::TEXTURE;
            entries[0].binding = 0;
            entries[0].texture = albedo_tex->view;

            entries[1].type    = GPUResourceType::TEXTURE;
            entries[1].binding = 1;
            entries[1].texture = normal_tex->view;

            entries[2].type    = GPUResourceType::TEXTURE;
            entries[2].binding = 2;
            entries[2].texture = mat_tex->view;

            entries[3].type    = GPUResourceType::TEXTURE;
            entries[3].binding = 3;
            entries[3].texture = depth_tex->view;

            entries[4].type          = GPUResourceType::BUFFER;
            entries[4].binding       = 4;
            entries[4].buffer.buffer = renderer->camera_buffers[current_slot];
            entries[4].buffer.offset = 0;
            entries[4].buffer.size   = sizeof(CameraUniforms);

            entries[5].type    = GPUResourceType::TEXTURE;
            entries[5].binding = 5;
            entries[5].texture = s_view;

            entries[6].type    = GPUResourceType::SAMPLER;
            entries[6].binding = 6;
            entries[6].sampler = renderer->shadow_sampler.handle;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.heap    = renderer->heap;
            bg_desc.layout  = renderer->blayouts.at(0);
            bg_desc.entries = entries;
            auto bg         = renderer->device.create_bind_group(bg_desc);

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = lit_tex->view;
            color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
            color_attachment.load_op     = GPULoadOp::CLEAR;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto pass_desc              = GPURenderPassDescriptor{};
            pass_desc.color_attachments = color_attachment;

            auto cmd = fg_ctx->cmdlist;
            cmd.begin_render_pass(pass_desc);
            cmd.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            cmd.set_scissor_rect(0, 0, extent.width, extent.height);
            cmd.set_pipeline(renderer->lighting_pipeline);
            cmd.set_bind_group(0, bg);
            cmd.draw(6, 1, 0, 0);
            cmd.end_render_pass();
        });

    auto final_color = lighting_data.lit_output;
    if (renderer->skybox_pass.valid()) {
        auto sky_data = renderer->skybox_pass.add(
            ctx.builder,
            lighting_data.lit_output,
            gbuffer.depth_stencil,
            view,
            ctx.scene,
            GPULoadOp::LOAD,
            GPULoadOp::LOAD);
        final_color = sky_data.color_output;
    }

    uint triangles = 0;
    for (const auto& mesh : ctx.scene.meshes) {
        triangles += mesh.index_count / 3;
    }

    renderer->stats.draw_calls         = static_cast<uint>(ctx.scene.meshes.size());
    renderer->stats.triangles_rendered = triangles;

    if (ctx.outputs) {
        ctx.outputs->depth = gbuffer.depth_stencil;
    }

    return final_color;
}

static void draw_debug_panel(RendererHandle handle)
{
    // debug panel UI
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
