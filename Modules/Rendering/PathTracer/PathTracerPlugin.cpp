#include <algorithm>
#include <cstring>
#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/Pass.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

namespace
{
    static Logger get_logger()
    {
        static Logger logger = create_logger("PathTracer", LogLevel::info);
        return logger;
    }

    struct alignas(16) PathTracerPushConstants
    {
        uint  width                  = 0;
        uint  height                 = 0;
        uint  sample_index           = 0;
        uint  max_bounces            = 4;
        uint  frame_seed             = 0;
        float exposure               = 1.0f;
        uint  debug_mode             = 0;
        uint  num_emissive_triangles = 0;
    };
    static_assert(sizeof(PathTracerPushConstants) == 32, "PathTracerPushConstants must be 32 bytes");

    struct alignas(16) AtrousPushConstants
    {
        uint  width     = 0;
        uint  height    = 0;
        int   step_size = 1;
        float sigma_l   = 4.0f;
        float sigma_n   = 32.0f;
        float sigma_z   = 1.0f;
        uint  iteration = 0;
        uint  reserved  = 0;
    };
    static_assert(sizeof(AtrousPushConstants) == 32, "AtrousPushConstants must be 32 bytes");
} // namespace

struct PathTracerRendererInstance
{
    RendererDescriptor descriptor;
    RendererSettings   settings;
    RendererStats      stats;

    // device, compiler & pipelines
    GPUDevice                        device;
    Compiler                         compiler;
    GPURayTracingPipeline            rt_pipeline;
    GPUPipelineLayout                rt_layout;
    GPUComputePipeline               path_trace_pipeline;
    Vector<GPUBindGroupLayoutHandle> blayouts;

    // atrous denoising pipeline
    GPUComputePipeline               atrous_pipeline;
    GPUPipelineLayout                atrous_layout;
    Vector<GPUBindGroupLayoutHandle> atrous_blayouts;

    // shader binding table & accumulation buffers
    GPUBuffer      sbt_buffer;
    GPUTexture     accumulation_tex;
    GPUTextureView accumulation_view;
    GPUExtent2D    accumulation_extent{0, 0};

    // fallback acceleration structure if scene lacks tlas
    GPUTlas   fallback_tlas;
    GPUBlas   fallback_blas;
    GPUBuffer fallback_scratch;
    GPUBuffer fallback_vbuffer;
    GPUBuffer fallback_ibuffer;
    GPUBuffer fallback_ebuffer;

    // temporal accumulation state
    uint      accumulated_frames = 0;
    Matrix4x4 prev_view_proj{0.0f};

    auto init(GPUDevice dev, Compiler comp) -> bool;
    void setup_sbt(GPUDevice dev);
    void setup_fallback_as(GPUDevice dev);
    void cleanup();
};

void PathTracerRendererInstance::setup_sbt(GPUDevice dev)
{
    if (sbt_buffer.handle.valid()) {
        sbt_buffer.destroy();
    }

    // construct shader binding table buffer for raygen, miss, and hitgroup records
    GPUBufferDescriptor sbt_desc{};
    sbt_desc.label = "path_tracer_sbt";
    sbt_desc.size  = 4096;
    sbt_desc.usage = GPUBufferUsage::STORAGE | GPUBufferUsage::UNIFORM;
    sbt_buffer     = dev.create_buffer(sbt_desc);
}

void PathTracerRendererInstance::setup_fallback_as(GPUDevice dev)
{
    if (fallback_tlas.handle.valid()) return;

    PathTracerVertex vertices[3] = {
        {Vector3(-1.0f, -1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(1.0f), Vector3(0.0f)},
        {Vector3(1.0f, -1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(1.0f), Vector3(0.0f)},
        {Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(1.0f), Vector3(0.0f)}};

    uint indices[3] = {0, 1, 2};

    EmissiveTriangle dummy_emissive{};

    GPUBufferDescriptor vdesc{};
    vdesc.label              = "path_tracer_fallback_vbuffer";
    vdesc.size               = sizeof(vertices);
    vdesc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
    vdesc.mapped_at_creation = true;
    fallback_vbuffer         = dev.create_buffer(vdesc);
    auto mapped              = fallback_vbuffer.get_mapped_range<PathTracerVertex>();
    std::memcpy(mapped.data, vertices, sizeof(vertices));
    fallback_vbuffer.unmap();

    GPUBufferDescriptor idesc{};
    idesc.label              = "path_tracer_fallback_ibuffer";
    idesc.size               = sizeof(indices);
    idesc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
    idesc.mapped_at_creation = true;
    fallback_ibuffer         = dev.create_buffer(idesc);
    auto imapped             = fallback_ibuffer.get_mapped_range<uint>();
    std::memcpy(imapped.data, indices, sizeof(indices));
    fallback_ibuffer.unmap();

    GPUBufferDescriptor edesc{};
    edesc.label              = "path_tracer_fallback_ebuffer";
    edesc.size               = sizeof(dummy_emissive);
    edesc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
    edesc.mapped_at_creation = true;
    fallback_ebuffer         = dev.create_buffer(edesc);
    auto emapped             = fallback_ebuffer.get_mapped_range<EmissiveTriangle>();
    std::memcpy(emapped.data, &dummy_emissive, sizeof(dummy_emissive));
    fallback_ebuffer.unmap();

    GPUBlasDescriptor bdesc{};
    bdesc.label = "path_tracer_fallback_blas";
    bdesc.flags = GPUBVHFlag::PREFER_FAST_TRACE;

    GPUBlasGeometrySizeDescriptor size{};
    size.type                    = GPUBlasType::TRIANGLE;
    size.triangles.flags         = GPUBVHGeometryFlag::BVH_OPAQUE;
    size.triangles.vertex_format = GPUVertexFormat::FLOAT32x3;
    size.triangles.vertex_count  = 3;
    fallback_blas                = dev.create_blas(bdesc, {size});

    GPUTlasDescriptor tdesc{};
    tdesc.label         = "path_tracer_fallback_tlas";
    tdesc.flags         = GPUBVHFlag::PREFER_FAST_TRACE;
    tdesc.max_instances = 1;
    fallback_tlas       = dev.create_tlas(tdesc);

    auto blas_sizes   = dev.get_blas_sizes(fallback_blas);
    auto tlas_sizes   = dev.get_tlas_sizes(fallback_tlas);
    auto scratch_size = std::max(blas_sizes.build_size, tlas_sizes.build_size);

    GPUBufferDescriptor sdesc{};
    sdesc.label           = "path_tracer_fallback_scratch";
    sdesc.size            = scratch_size > 0 ? scratch_size : 4096;
    sdesc.usage           = GPUBufferUsage::STORAGE;
    sdesc.virtual_address = true;
    fallback_scratch      = dev.create_buffer(sdesc);

    GPUCommandBufferDescriptor cdesc{};
    cdesc.queue = GPUQueueType::DEFAULT;
    auto cmd    = dev.create_command_buffer(cdesc);

    GPUBlasTriangleGeometry tri{};
    tri.size.flags         = GPUBVHGeometryFlag::BVH_OPAQUE;
    tri.size.vertex_format = GPUVertexFormat::FLOAT32x3;
    tri.size.vertex_count  = 3;
    tri.vertex_buffer      = fallback_vbuffer;
    tri.vertex_stride      = sizeof(PathTracerVertex);

    GPUBlasBuildEntry b_entry{};
    b_entry.blas                 = fallback_blas;
    b_entry.geometries.type      = GPUBlasType::TRIANGLE;
    b_entry.geometries.triangles = tri;
    cmd.build_blases(fallback_scratch, b_entry);

    GPUTlasInstance inst{};
    std::memset(inst.transform, 0, sizeof(inst.transform));
    inst.transform[0][0] = 1.0f;
    inst.transform[1][1] = 1.0f;
    inst.transform[2][2] = 1.0f;
    inst.mask            = 0xFF;
    inst.blas            = fallback_blas;

    Vector<GPUTlasInstance> insts;
    insts.push_back(inst);

    GPUTlasBuildEntry t_entry{};
    t_entry.tlas      = fallback_tlas;
    t_entry.instances = insts;
    cmd.build_tlases(fallback_scratch, t_entry);
    cmd.submit();
    dev.wait();
}

auto PathTracerRendererInstance::init(GPUDevice dev, Compiler comp) -> bool
{
    if (!dev.valid() || !comp.handle.valid()) return false;
    device   = dev;
    compiler = comp;

    // 1. compile path tracer shader
    auto module = execute([&]() {
        CompileDescriptor desc{};
        desc.module = "path_tracer";
        desc.path   = "sys://Shaders/PathTracer/PathTracer.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        get_logger()->error("Failed to compile PathTracer shader: sys://Shaders/PathTracer/PathTracer.slang");
        return false;
    }

    // 2. reflect compute entry point
    auto reflection = compiler.reflect({
        {*module, "csmain"},
    });
    if (!reflection) {
        get_logger()->error("Failed to reflect csmain in PathTracer.slang");
        return false;
    }

    // 3. create bind group layouts and pipeline layout
    blayouts.clear();
    auto blayout_descs = reflection->get_bind_group_layouts();
    for (const auto& bg_desc : blayout_descs) {
        blayouts.push_back(device.create_bind_group_layout(bg_desc));
    }

    GPUPipelineLayoutDescriptor playout_desc{};
    playout_desc.label              = "path_tracer_playout";
    playout_desc.bind_group_layouts = blayouts;
    playout_desc.immediate_size     = reflection->get_immediate_size();
    rt_layout                       = device.create_pipeline_layout(playout_desc);

    // 4. create compute pipeline
    auto code = module->get_shader_blob("csmain");
    if (code) {
        GPUShaderModuleDescriptor sm_desc{};
        sm_desc.label = "path_tracer_cs_module";
        sm_desc.data  = code->data;
        sm_desc.size  = code->size;
        auto sm       = device.create_shader_module(sm_desc);

        GPUComputePipelineDescriptor cp_desc{};
        cp_desc.label               = "path_tracer_compute_pipeline";
        cp_desc.layout              = rt_layout;
        cp_desc.compute.module      = sm;
        cp_desc.compute.entry_point = "csmain";
        path_trace_pipeline         = device.create_compute_pipeline(cp_desc);
    }

    // 5. create hardware ray tracing pipeline
    GPURayTracingPipelineDescriptor rt_desc{};
    rt_desc.label               = "path_tracer_rt_pipeline";
    rt_desc.layout              = rt_layout;
    rt_desc.max_recursion_depth = settings.max_ray_bounces;
    rt_pipeline                 = device.create_raytracing_pipeline(rt_desc);

    // 6. construct shader binding table
    setup_sbt(device);

    // 7. compile and reflect atrous denoising compute shader
    auto atrous_module = execute([&]() {
        CompileDescriptor desc{};
        desc.module = "atrous_denoise";
        desc.path   = "sys://Shaders/PathTracer/AtrousDenoise.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (atrous_module) {
        auto atrous_reflection = compiler.reflect({
            {*atrous_module, "cs_atrous"},
        });
        if (atrous_reflection) {
            atrous_blayouts.clear();
            for (const auto& bg_desc : atrous_reflection->get_bind_group_layouts()) {
                atrous_blayouts.push_back(device.create_bind_group_layout(bg_desc));
            }

            GPUPipelineLayoutDescriptor playout_desc{};
            playout_desc.label              = "atrous_playout";
            playout_desc.bind_group_layouts = atrous_blayouts;
            playout_desc.immediate_size     = atrous_reflection->get_immediate_size();
            atrous_layout                   = device.create_pipeline_layout(playout_desc);

            auto atrous_code = atrous_module->get_shader_blob("cs_atrous");
            if (atrous_code) {
                GPUShaderModuleDescriptor sm_desc{};
                sm_desc.label = "atrous_cs_module";
                sm_desc.data  = atrous_code->data;
                sm_desc.size  = atrous_code->size;
                auto sm       = device.create_shader_module(sm_desc);

                GPUComputePipelineDescriptor cp_desc{};
                cp_desc.label               = "atrous_compute_pipeline";
                cp_desc.layout              = atrous_layout;
                cp_desc.compute.module      = sm;
                cp_desc.compute.entry_point = "cs_atrous";
                atrous_pipeline             = device.create_compute_pipeline(cp_desc);
            }
        }
    }

    return true;
}

void PathTracerRendererInstance::cleanup()
{
    if (device.valid()) {
        device.wait();
        if (atrous_pipeline.valid()) atrous_pipeline.destroy();
        for (auto& layout : atrous_blayouts) {
            if (layout.valid()) GPUBindGroupLayout(layout).destroy();
        }
        atrous_blayouts.clear();
        if (atrous_layout.valid()) atrous_layout.destroy();
        if (accumulation_view.valid()) accumulation_view.destroy();
        if (accumulation_tex.valid()) accumulation_tex.destroy();
        if (sbt_buffer.valid()) sbt_buffer.destroy();
        if (rt_pipeline.valid()) rt_pipeline.destroy();
        if (path_trace_pipeline.valid()) path_trace_pipeline.destroy();
        for (auto& layout : blayouts) {
            if (layout.valid()) GPUBindGroupLayout(layout).destroy();
        }
        blayouts.clear();
        if (rt_layout.valid()) rt_layout.destroy();
        if (fallback_tlas.valid()) fallback_tlas.destroy();
        if (fallback_blas.valid()) fallback_blas.destroy();
        if (fallback_scratch.valid()) fallback_scratch.destroy();
        if (fallback_vbuffer.valid()) fallback_vbuffer.destroy();
        if (fallback_ibuffer.valid()) fallback_ibuffer.destroy();
        if (fallback_ebuffer.valid()) fallback_ebuffer.destroy();
    }
}

static auto get_name() -> CString
{
    return "PathTracer";
}

static auto get_capabilities() -> RendererCapabilities
{
    RendererCapabilities caps{};
    caps.features               = RendererFeature::HARDWARE_RAYTRACING |
                                  RendererFeature::ACCUMULATION |
                                  RendererFeature::POST_PROCESSING |
                                  RendererFeature::DEBUG_VISUALIZATION;
    caps.supported_primitives   = PrimitiveSupport::BVH |
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
    instance->compiler   = descriptor.compiler;

    if (RHI::api() && descriptor.compiler.handle.valid() && descriptor.device.valid()) {
        instance->init(descriptor.device, descriptor.compiler);
    }

    out_handle = RendererHandle(instance.release());
    return true;
}

static void delete_renderer(RendererHandle handle)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
        ptr->cleanup();
        delete ptr;
    }
}

static void resize(RendererHandle handle, uint width, uint height)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
        ptr->descriptor.initial_extent = {width, height};
        if (ptr->accumulation_extent.width != width || ptr->accumulation_extent.height != height) {
            if (ptr->accumulation_view.valid()) ptr->accumulation_view.destroy();
            if (ptr->accumulation_tex.valid()) ptr->accumulation_tex.destroy();
            ptr->accumulation_extent = {0, 0};
        }
        ptr->accumulated_frames = 0;
    }
}

static void set_settings(RendererHandle handle, const RendererSettings& settings)
{
    if (auto ptr = handle.as_type<PathTracerRendererInstance>()) {
        if (ptr->settings.max_ray_bounces != settings.max_ray_bounces ||
            ptr->settings.exposure != settings.exposure ||
            ptr->settings.debug_mode != settings.debug_mode ||
            ptr->settings.enable_denoiser != settings.enable_denoiser ||
            ptr->settings.denoiser_iterations != settings.denoiser_iterations) {
            ptr->accumulated_frames = 0;
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

static FrameGraph::TextureHandle render(
    RendererHandle            handle,
    const RenderFrameContext& ctx)
{
    auto renderer = handle.as_type<PathTracerRendererInstance>();
    if (!renderer) {
        return ctx.color_target;
    }

    GPUExtent2D extent = (ctx.render_extent.width > 0 && ctx.render_extent.height > 0)
                             ? ctx.render_extent
                             : ctx.display_extent;

    if (!ctx.color_target.valid() || extent.width == 0 || extent.height == 0) {
        return ctx.color_target;
    }

    // reset temporal accumulation if camera moved
    Matrix4x4 cur_view_proj = ctx.view.projection * ctx.view.view;
    if (cur_view_proj != renderer->prev_view_proj) {
        renderer->accumulated_frames = 0;
        renderer->prev_view_proj     = cur_view_proj;
    }

    // lazy initialization if needed
    if (!renderer->path_trace_pipeline.valid() && !renderer->rt_pipeline.valid()) {
        auto comp = renderer->compiler.handle.valid()
                        ? renderer->compiler
                        : (ctx.services ? *ctx.services->try_get<Compiler*>() : Compiler{});
        if (comp.handle.valid()) {
            renderer->init(renderer->device, comp);
        }
    }

    // allocate or resize accumulation buffer
    if (!renderer->accumulation_tex.valid() ||
        renderer->accumulation_extent.width != extent.width ||
        renderer->accumulation_extent.height != extent.height) {
        if (renderer->accumulation_view.valid()) renderer->accumulation_view.destroy();
        if (renderer->accumulation_tex.valid()) renderer->accumulation_tex.destroy();

        GPUTextureDescriptor acc_desc{};
        acc_desc.label                = "path_tracer_accumulation";
        acc_desc.dimension            = GPUTextureDimension::x2D;
        acc_desc.size                 = GPUExtent3D{extent.width, extent.height, 1};
        acc_desc.format               = GPUTextureFormat::RGBA32FLOAT;
        acc_desc.usage                = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
        renderer->accumulation_tex    = renderer->device.create_texture(acc_desc);
        renderer->accumulation_view   = renderer->accumulation_tex.create_view();
        renderer->accumulation_extent = extent;
        renderer->accumulated_frames  = 0;
    }

    // resolve active TLAS
    GPUTlasHandle active_tlas = ctx.scene.scene_tlas;
    if (!active_tlas.valid()) {
        renderer->setup_fallback_as(renderer->device);
        active_tlas = renderer->fallback_tlas;
    }

    bool use_denoiser = renderer->settings.enable_denoiser &&
                        renderer->settings.denoiser_iterations > 0 &&
                        renderer->atrous_pipeline.valid();

    struct PathTracerPassData
    {
        FrameGraphHandle<FrameGraphTexture> output;
    };

    struct AtrousPassData
    {
        FrameGraphHandle<FrameGraphTexture> input;
        FrameGraphHandle<FrameGraphTexture> output;
    };

    FrameGraphHandle<FrameGraphTexture> final_output;

    if (!use_denoiser) {
        auto pass_data = ctx.builder.add_pass<PathTracerPassData>(
            "PathTracerPass",
            [&](PathTracerPassData& data, FrameGraphPassBuilder& pass_builder) {
                data.output = pass_builder.write(ctx.color_target);
                pass_builder.preserve();
            },
            [renderer, extent, &view = ctx.view, &scene = ctx.scene, active_tlas](const PathTracerPassData& data, FrameGraphResources& resources, FrameGraphContext* fg_ctx) {
                if (!fg_ctx || !fg_ctx->cmdlist.handle.valid()) return;
                if (!renderer->path_trace_pipeline.valid() && !renderer->rt_pipeline.valid()) return;

                auto out_tex = resources.get<FrameGraphTexture>(data.output);
                if (!out_tex) return;

                renderer->setup_fallback_as(renderer->device);

                PathTracerPushConstants push{};
                push.width                  = extent.width;
                push.height                 = extent.height;
                push.sample_index           = renderer->accumulated_frames;
                push.max_bounces            = renderer->settings.max_ray_bounces;
                push.frame_seed             = renderer->accumulated_frames * 1999u + 1u;
                push.exposure               = renderer->settings.exposure > 0.0f ? renderer->settings.exposure : 1.0f;
                push.debug_mode             = renderer->settings.debug_mode;
                push.num_emissive_triangles = scene.emissive_triangle_count;

                auto cam_uniforms = CameraUniforms::from_view(view);
                auto cam_slice    = fg_ctx->create_uniform(cam_uniforms, 256);

                BufferSlice vbuf = scene.scene_vbuffer.valid() ? scene.scene_vbuffer : BufferSlice{renderer->fallback_vbuffer, 0, sizeof(PathTracerVertex) * 3};
                BufferSlice ibuf = scene.scene_ibuffer.valid() ? scene.scene_ibuffer : BufferSlice{renderer->fallback_ibuffer, 0, sizeof(uint) * 3};
                BufferSlice ebuf = scene.emissive_triangles.valid() ? scene.emissive_triangles : BufferSlice{renderer->fallback_ebuffer, 0, sizeof(EmissiveTriangle)};

                Array<GPUBindGroupEntry, 7> entries{};
                entries[0].type          = GPUResourceType::BUFFER;
                entries[0].binding       = 0;
                entries[0].buffer.buffer = cam_slice.buffer;
                entries[0].buffer.offset = cam_slice.offset;
                entries[0].buffer.size   = sizeof(CameraUniforms);

                entries[1].type    = GPUResourceType::ACCELERATION_STRUCTURE;
                entries[1].binding = 1;
                entries[1].tlas    = active_tlas;

                entries[2].type    = GPUResourceType::STORAGE_TEXTURE;
                entries[2].binding = 2;
                entries[2].texture = renderer->accumulation_view;

                entries[3].type    = GPUResourceType::STORAGE_TEXTURE;
                entries[3].binding = 3;
                entries[3].texture = out_tex->view;

                entries[4].type          = GPUResourceType::BUFFER;
                entries[4].binding       = 4;
                entries[4].buffer.buffer = vbuf.buffer;
                entries[4].buffer.offset = vbuf.offset;
                entries[4].buffer.size   = vbuf.size;

                entries[5].type          = GPUResourceType::BUFFER;
                entries[5].binding       = 5;
                entries[5].buffer.buffer = ibuf.buffer;
                entries[5].buffer.offset = ibuf.offset;
                entries[5].buffer.size   = ibuf.size;

                entries[6].type          = GPUResourceType::BUFFER;
                entries[6].binding       = 6;
                entries[6].buffer.buffer = ebuf.buffer;
                entries[6].buffer.offset = ebuf.offset;
                entries[6].buffer.size   = ebuf.size;

                GPUBindGroupDescriptor bg_desc{};
                bg_desc.layout  = renderer->blayouts.empty() ? GPUBindGroupLayoutHandle{} : renderer->blayouts[0];
                bg_desc.entries = entries;
                auto bg         = fg_ctx->allocate(bg_desc);

                auto command = fg_ctx->cmdlist;
                if (renderer->rt_pipeline.valid()) {
                    command.set_pipeline(renderer->rt_pipeline);
                }
                if (renderer->path_trace_pipeline.valid()) {
                    command.set_pipeline(renderer->path_trace_pipeline);
                }
                command.set_bind_group(0, bg);
                command.set_immediates(0, push);
                command.dispatch_workgroups((extent.width + 7) / 8, (extent.height + 7) / 8, 1);
            });

        final_output = pass_data.output;
    } else {
        GPUTextureDescriptor raw_desc{};
        raw_desc.label           = "path_tracer_raw";
        raw_desc.dimension       = GPUTextureDimension::x2D;
        raw_desc.size            = GPUExtent3D{extent.width, extent.height, 1};
        raw_desc.format          = GPUTextureFormat::RGBA16FLOAT;
        raw_desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
        raw_desc.mip_level_count = 1;

        auto raw_pass = ctx.builder.add_pass<PathTracerPassData>(
            "PathTracerPass",
            [&](PathTracerPassData& data, FrameGraphPassBuilder& pass_builder) {
                data.output = pass_builder.create<FrameGraphTexture>(raw_desc);
                data.output = pass_builder.write(data.output);
            },
            [renderer, extent, &view = ctx.view, &scene = ctx.scene, active_tlas](const PathTracerPassData& data, FrameGraphResources& resources, FrameGraphContext* fg_ctx) {
                if (!fg_ctx || !fg_ctx->cmdlist.handle.valid()) return;
                if (!renderer->path_trace_pipeline.valid() && !renderer->rt_pipeline.valid()) return;

                auto out_tex = resources.get<FrameGraphTexture>(data.output);
                if (!out_tex) return;

                renderer->setup_fallback_as(renderer->device);

                PathTracerPushConstants push{};
                push.width                  = extent.width;
                push.height                 = extent.height;
                push.sample_index           = renderer->accumulated_frames;
                push.max_bounces            = renderer->settings.max_ray_bounces;
                push.frame_seed             = renderer->accumulated_frames * 1999u + 1u;
                push.exposure               = renderer->settings.exposure > 0.0f ? renderer->settings.exposure : 1.0f;
                push.debug_mode             = renderer->settings.debug_mode;
                push.num_emissive_triangles = scene.emissive_triangle_count;

                auto cam_uniforms = CameraUniforms::from_view(view);
                auto cam_slice    = fg_ctx->create_uniform(cam_uniforms, 256);

                BufferSlice vbuf = scene.scene_vbuffer.valid() ? scene.scene_vbuffer : BufferSlice{renderer->fallback_vbuffer, 0, sizeof(PathTracerVertex) * 3};
                BufferSlice ibuf = scene.scene_ibuffer.valid() ? scene.scene_ibuffer : BufferSlice{renderer->fallback_ibuffer, 0, sizeof(uint) * 3};
                BufferSlice ebuf = scene.emissive_triangles.valid() ? scene.emissive_triangles : BufferSlice{renderer->fallback_ebuffer, 0, sizeof(EmissiveTriangle)};

                Array<GPUBindGroupEntry, 7> entries{};
                entries[0].type          = GPUResourceType::BUFFER;
                entries[0].binding       = 0;
                entries[0].buffer.buffer = cam_slice.buffer;
                entries[0].buffer.offset = cam_slice.offset;
                entries[0].buffer.size   = sizeof(CameraUniforms);

                entries[1].type    = GPUResourceType::ACCELERATION_STRUCTURE;
                entries[1].binding = 1;
                entries[1].tlas    = active_tlas;

                entries[2].type    = GPUResourceType::STORAGE_TEXTURE;
                entries[2].binding = 2;
                entries[2].texture = renderer->accumulation_view;

                entries[3].type    = GPUResourceType::STORAGE_TEXTURE;
                entries[3].binding = 3;
                entries[3].texture = out_tex->view;

                entries[4].type          = GPUResourceType::BUFFER;
                entries[4].binding       = 4;
                entries[4].buffer.buffer = vbuf.buffer;
                entries[4].buffer.offset = vbuf.offset;
                entries[4].buffer.size   = vbuf.size;

                entries[5].type          = GPUResourceType::BUFFER;
                entries[5].binding       = 5;
                entries[5].buffer.buffer = ibuf.buffer;
                entries[5].buffer.offset = ibuf.offset;
                entries[5].buffer.size   = ibuf.size;

                entries[6].type          = GPUResourceType::BUFFER;
                entries[6].binding       = 6;
                entries[6].buffer.buffer = ebuf.buffer;
                entries[6].buffer.offset = ebuf.offset;
                entries[6].buffer.size   = ebuf.size;

                GPUBindGroupDescriptor bg_desc{};
                bg_desc.layout  = renderer->blayouts.empty() ? GPUBindGroupLayoutHandle{} : renderer->blayouts[0];
                bg_desc.entries = entries;
                auto bg         = fg_ctx->allocate(bg_desc);

                auto command = fg_ctx->cmdlist;
                if (renderer->rt_pipeline.valid()) {
                    command.set_pipeline(renderer->rt_pipeline);
                }
                if (renderer->path_trace_pipeline.valid()) {
                    command.set_pipeline(renderer->path_trace_pipeline);
                }
                command.set_bind_group(0, bg);
                command.set_immediates(0, push);
                command.dispatch_workgroups((extent.width + 7) / 8, (extent.height + 7) / 8, 1);
            });

        uint total_iters = std::clamp(renderer->settings.denoiser_iterations, 1u, 5u);

        FrameGraphHandle<FrameGraphTexture> current_src = raw_pass.output;

        for (uint i = 0; i < total_iters; ++i) {
            bool is_last = (i == total_iters - 1);
            int  step    = 1 << i;

            String pass_name = "AtrousDenoise_" + std::to_string(i);

            auto atrous_pass = ctx.builder.add_pass<AtrousPassData>(
                pass_name,
                [&, current_src, is_last, i](AtrousPassData& data, FrameGraphPassBuilder& pass_builder) {
                    data.input = pass_builder.sample(current_src);
                    if (is_last) {
                        data.output = pass_builder.write(ctx.color_target);
                        pass_builder.preserve();
                    } else {
                        auto desc   = raw_desc;
                        desc.label  = "atrous_step";
                        data.output = pass_builder.create<FrameGraphTexture>(desc);
                        data.output = pass_builder.write(data.output);
                    }
                },
                [renderer, extent, i, step](const AtrousPassData& data, FrameGraphResources& resources, FrameGraphContext* fg_ctx) {
                    if (!fg_ctx || !fg_ctx->cmdlist.handle.valid()) return;

                    auto in_tex  = resources.get<FrameGraphTexture>(data.input);
                    auto out_tex = resources.get<FrameGraphTexture>(data.output);
                    if (!in_tex || !out_tex) return;

                    AtrousPushConstants push{};
                    push.width     = extent.width;
                    push.height    = extent.height;
                    push.step_size = step;
                    push.sigma_l   = 4.0f;
                    push.sigma_n   = 32.0f;
                    push.sigma_z   = 1.0f;
                    push.iteration = i;

                    Array<GPUBindGroupEntry, 2> entries{};
                    entries[0].type    = GPUResourceType::TEXTURE;
                    entries[0].binding = 0;
                    entries[0].texture = in_tex->view;

                    entries[1].type    = GPUResourceType::STORAGE_TEXTURE;
                    entries[1].binding = 1;
                    entries[1].texture = out_tex->view;

                    GPUBindGroupDescriptor bg_desc{};
                    bg_desc.layout  = renderer->atrous_blayouts[0];
                    bg_desc.entries = entries;
                    auto bg         = fg_ctx->allocate(bg_desc);

                    auto command = fg_ctx->cmdlist;
                    command.set_pipeline(renderer->atrous_pipeline);
                    command.set_bind_group(0, bg);
                    command.set_immediates(0, push);
                    command.dispatch_workgroups((extent.width + 7) / 8, (extent.height + 7) / 8, 1);
                    renderer->stats.dispatch_calls++;
                });

            current_src  = atrous_pass.output;
            final_output = atrous_pass.output;
        }
    }

    uint triangles = 0;
    for (const auto& mesh : ctx.scene.meshes) {
        triangles += mesh.index_count / 3;
    }

    renderer->accumulated_frames++;
    renderer->stats.rays_traced        = extent.width * extent.height * renderer->settings.samples_per_pixel;
    renderer->stats.triangles_rendered = triangles;
    renderer->stats.dispatch_calls++;

    if (ctx.outputs && ctx.depth_target.valid()) {
        ctx.outputs->depth = ctx.depth_target;
    }

    return final_output;
}

static void draw_debug_panel(RendererHandle handle)
{
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
