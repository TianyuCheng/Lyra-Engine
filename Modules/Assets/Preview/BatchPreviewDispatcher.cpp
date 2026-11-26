#include <cmath>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <stb_image_write.h>
#include <stb_image_resize2.h>
#include <cmrc/cmrc.hpp>

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/JobSystem/JobScheduler.h>
#include "BatchPreviewDispatcher.h"

CMRC_DECLARE(preview);

using namespace lyra;
using namespace lyra::preview;

namespace fs = std::filesystem;

static Logger get_logger()
{
    static Logger logger = create_logger("PreviewBatch", LogLevel::trace);
    return logger;
}

static PipelinePreviewConfig G_CONFIG;

PipelinePreviewConfig& lyra::preview::get_config()
{
    return G_CONFIG;
}

static Own<BatchPreviewDispatcher> G_DISPATCHER_INSTANCE;

BatchPreviewDispatcher& BatchPreviewDispatcher::instance()
{
    if (!G_DISPATCHER_INSTANCE) {
        G_DISPATCHER_INSTANCE = make_own<BatchPreviewDispatcher>();
    }
    return *G_DISPATCHER_INSTANCE;
}

BatchPreviewDispatcher::~BatchPreviewDispatcher()
{
    if (gpu_initialized) {
        destroy_gpu_resources();
    }
}

void BatchPreviewDispatcher::configure(const AssetConfigContext& context)
{
    UniqueLock lock(mutex);

    if (context.desc_data && context.desc_type == PreviewDescriptor::type) {
        const auto& desc = *reinterpret_cast<const PreviewDescriptor*>(context.desc_data);
        if (desc.caches_root) {
            G_CONFIG.caches_root = desc.caches_root;
        }
        if (desc.width > 0) {
            G_CONFIG.width = desc.width;
        }
        if (desc.height > 0) {
            G_CONFIG.height = desc.height;
        }
        G_CONFIG.pitch_deg   = desc.pitch_deg;
        G_CONFIG.yaw_deg     = desc.yaw_deg;
        G_CONFIG.fov_deg     = desc.fov_deg;
        G_CONFIG.clear_color = desc.clear_color;
        G_CONFIG.enable_ssaa = desc.enable_ssaa;

        if (desc.compiler.valid()) {
            external_compiler = desc.compiler;
        }
    }
}

auto BatchPreviewDispatcher::generate_thumbnail(
    const PreviewScene& scene,
    JSON&               metadata) -> Future<Path>
{
    if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
        Promise<Path> p;
        p.set_value(Path());
        return p.get_future();
    }

    auto guid        = metadata["guid"].get<AssetID>();
    Path caches_root = G_CONFIG.caches_root;
    if (caches_root.empty()) {
        get_logger()->error("Preview generator caches_root not configured!");
        Promise<Path> p;
        p.set_value(Path());
        return p.get_future();
    }

    Path            thumb_dir = caches_root / "thumbnails";
    std::error_code ec;
    fs::create_directories(thumb_dir, ec);

    Path thumb_path = thumb_dir / (std::to_string(guid) + ".thumb.png");
    Path rel_path   = fs::relative(thumb_path, caches_root);

    // immediately populate deterministic thumbnail path into metadata for cooker commit
    metadata["thumbnail"] = rel_path.generic_string();

    // fast path: 2D flat textured quad (image/texture preview)
    if (scene.is_flat && !scene.textures.empty()) {
        const auto& tex = scene.textures[0];
        if (tex.width > 0 && tex.height > 0 && !tex.pixels.empty()) {
            int thumb_w = static_cast<int>(G_CONFIG.width);
            int thumb_h = static_cast<int>(G_CONFIG.height);

            if (tex.width > tex.height) {
                thumb_h = std::max(1, static_cast<int>(static_cast<float>(tex.height) / tex.width * G_CONFIG.width));
            } else {
                thumb_w = std::max(1, static_cast<int>(static_cast<float>(tex.width) / tex.height * G_CONFIG.height));
            }

            Vector<uint8_t> resized(thumb_w * thumb_h * 4);
            stbir_resize_uint8_linear(
                tex.pixels.data(), tex.width, tex.height, 0,
                resized.data(), thumb_w, thumb_h, 0,
                STBIR_RGBA);

            if (!stbi_write_png(thumb_path.string().c_str(), thumb_w, thumb_h, 4, resized.data(), thumb_w * 4)) {
                get_logger()->warn("Failed to write preview thumbnail: {}", thumb_path.string());
            }
            Promise<Path> p;
            p.set_value(rel_path);
            return p.get_future();
        }
    }

    // queue for batched GPU rasterization
    auto promise = make_ref<Promise<Path>>();
    auto future  = promise->get_future();

    PreviewRequest req;
    req.guid       = guid;
    req.scene      = scene;
    req.thumb_path = thumb_path;
    req.rel_path   = rel_path;
    req.promise    = promise;

    {
        UniqueLock lock(mutex);
        pending_queue.push_back(std::move(req));

        if (pending_queue.size() >= MAX_BATCH_SIZE) {
            dispatch_batch_locked();
        } else if (pending_queue.size() == 1 && !timer_armed) {
            arm_debounce_timer_locked();
        }
    }

    return future;
}

auto BatchPreviewDispatcher::render_scene(const PreviewScene& scene) -> PreviewTexture
{
    // flush any pending batches
    flush();

    PreviewTexture result;
    result.width    = G_CONFIG.width;
    result.height   = G_CONFIG.height;
    result.channels = 4;
    result.pixels.resize(result.width * result.height * 4, 0);

    // fast path: 2D flat textured quad
    if (scene.is_flat && !scene.textures.empty()) {
        const auto& tex = scene.textures[0];
        if (tex.width > 0 && tex.height > 0 && !tex.pixels.empty()) {
            int thumb_w = static_cast<int>(G_CONFIG.width);
            int thumb_h = static_cast<int>(G_CONFIG.height);

            if (tex.width > tex.height) {
                thumb_h = std::max(1, static_cast<int>(static_cast<float>(tex.height) / tex.width * G_CONFIG.width));
            } else {
                thumb_w = std::max(1, static_cast<int>(static_cast<float>(tex.width) / tex.height * G_CONFIG.height));
            }

            Vector<uint8_t> resized(thumb_w * thumb_h * 4);
            stbir_resize_uint8_linear(
                tex.pixels.data(), tex.width, tex.height, 0,
                resized.data(), thumb_w, thumb_h, 0,
                STBIR_RGBA);

            result.width  = thumb_w;
            result.height = thumb_h;
            result.pixels = std::move(resized);
            return result;
        }
    }

    return result;
}

void BatchPreviewDispatcher::flush()
{
    UniqueLock lock(mutex);
    if (!pending_queue.empty()) {
        dispatch_batch_locked();
    }
}

void BatchPreviewDispatcher::shutdown()
{
    if (G_DISPATCHER_INSTANCE) {
        G_DISPATCHER_INSTANCE->flush();
        {
            UniqueLock lock(G_DISPATCHER_INSTANCE->mutex);
            G_DISPATCHER_INSTANCE->destroy_gpu_resources();
        }
        G_DISPATCHER_INSTANCE.reset();
    }
}

void BatchPreviewDispatcher::arm_debounce_timer_locked()
{
    timer_armed = true;
    JobScheduler::schedule(JobPriority::BACKGROUND, [this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        UniqueLock lock(mutex);
        timer_armed = false;
        if (!pending_queue.empty()) {
            dispatch_batch_locked();
        }
    });
}

auto BatchPreviewDispatcher::init_gpu_resources() -> bool
{
    if (gpu_initialized) return true;
    if (gpu_failed) return false;

    if (!RHI::api()) {
        gpu_failed = true;
        return false;
    }

    try {
        auto& device = RHI::get_current_device();

        // 1. initialize shader compiler
        if (external_compiler.valid()) {
            compiler = Compiler(external_compiler);
        } else {
            CompilerDescriptor cdesc{};
            StringView         api_name = RHI::api()->get_api_name();
            if (api_name == "MockGPU") {
                // MockGPU does not rasterize pixels; use mock thumbnail fallback
                return false;
            }

            if (api_name == "Vulkan") {
                cdesc.target = CompileTarget::SPIRV;
            } else if (api_name == "D3D12") {
                cdesc.target = CompileTarget::DXIL;
            } else if (api_name == "Metal") {
                cdesc.target = CompileTarget::MSL;
            } else if (api_name == "SoftGPU") {
                cdesc.target = CompileTarget::HOST;
            } else {
                cdesc.target = CompileTarget::SPIRV;
            }

            owned_compiler = Compiler::init(cdesc);
            if (!owned_compiler || !owned_compiler->handle.valid()) {
                get_logger()->warn("Compiler not available for GPU preview rasterizer");
                gpu_failed = true;
                return false;
            }
            compiler = *owned_compiler;
        }

        // 2. compile preview shader from embedded cmrc resource
        auto   res_fs      = cmrc::preview::get_filesystem();
        auto   shader_file = res_fs.open("Shaders/PreviewRasterizer.slang");
        String shader_src(shader_file.begin(), shader_file.end());

        CompileDescriptor sdesc{};
        sdesc.module = "preview_rasterizer";
        sdesc.path   = "PreviewRasterizer.slang";
        sdesc.source = shader_src.c_str();

        auto module = compiler.compile(sdesc);
        if (!module) {
            get_logger()->warn("Failed to compile preview shader module");
            gpu_failed = true;
            return false;
        }

        auto reflection = compiler.reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });
        if (!reflection) {
            get_logger()->warn("Failed to reflect preview shader");
            gpu_failed = true;
            return false;
        }

        // 3. bind group layout & pipeline layout
        auto                             blayout_descs = reflection->get_bind_group_layouts();
        Vector<GPUBindGroupLayoutHandle> blayouts;
        if (!blayout_descs.empty()) {
            bind_group_layout = device.create_bind_group_layout(blayout_descs[0]);
            blayouts.push_back(bind_group_layout.handle);
        }

        GPUPipelineLayoutDescriptor playout_desc{};
        playout_desc.label              = "preview_pipeline_layout";
        playout_desc.bind_group_layouts = Span<GPUBindGroupLayoutHandle>(blayouts);
        playout_desc.immediate_size     = reflection->get_immediate_size();
        pipeline_layout                 = device.create_pipeline_layout(playout_desc);

        // 4. bind group heap, sampler, and default white texture
        GPUBindGroupHeapDescriptor heap_desc{};
        heap_desc.page_size = 2048;
        bind_group_heap     = device.create_bind_group_heap(heap_desc);

        GPUSamplerDescriptor smp_desc{};
        smp_desc.label          = "preview_sampler";
        smp_desc.min_filter     = GPUFilterMode::LINEAR;
        smp_desc.mag_filter     = GPUFilterMode::LINEAR;
        smp_desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
        smp_desc.address_mode_u = GPUAddressMode::REPEAT;
        smp_desc.address_mode_v = GPUAddressMode::REPEAT;
        smp_desc.address_mode_w = GPUAddressMode::REPEAT;
        default_sampler         = device.create_sampler(smp_desc);

        GPUTextureDescriptor white_desc{};
        white_desc.label           = "preview_white_fallback";
        white_desc.format          = GPUTextureFormat::RGBA8UNORM;
        white_desc.dimension       = GPUTextureDimension::x2D;
        white_desc.size            = GPUExtent3D{1, 1, 1};
        white_desc.array_layers    = 1;
        white_desc.mip_level_count = 1;
        white_desc.sample_count    = 1;
        white_desc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        default_white_texture      = device.create_texture(white_desc);
        default_white_view         = default_white_texture.create_view();

        GPUBufferDescriptor sb_desc{};
        sb_desc.size    = 256;
        sb_desc.usage   = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
        auto staging_sb = device.create_buffer(sb_desc);
        staging_sb.map(GPUMapMode::WRITE, 0, 256);
        auto     mrange      = staging_sb.get_mapped_range<uint8_t>();
        uint32_t white_pixel = 0xFFFFFFFF;
        std::memcpy(mrange.data, &white_pixel, 4);
        staging_sb.unmap();

        auto init_cmd = device.create_command_buffer(GPUCommandBufferDescriptor{});
        init_cmd.resource_barrier(state_transition(default_white_texture, undefined_texture_state(), copy_dst_texture_state()));
        GPUTexelCopyBufferInfo sb_src{};
        sb_src.buffer         = staging_sb;
        sb_src.bytes_per_row  = 256;
        sb_src.rows_per_image = 1;
        GPUTexelCopyTextureInfo sb_dst{};
        sb_dst.texture = default_white_texture;
        sb_dst.aspect  = GPUTextureAspect::COLOR;
        init_cmd.copy_buffer_to_texture(sb_src, sb_dst, GPUExtent3D{1, 1, 1});
        init_cmd.resource_barrier(state_transition(default_white_texture, copy_dst_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        init_cmd.submit();
        device.wait();
        staging_sb.destroy();

        Array<GPUBindGroupEntry, 2> bg_entries{};
        bg_entries[0].type    = GPUResourceType::TEXTURE;
        bg_entries[0].binding = 0;
        bg_entries[0].texture = default_white_view;

        bg_entries[1].type    = GPUResourceType::SAMPLER;
        bg_entries[1].binding = 1;
        bg_entries[1].sampler = default_sampler;

        GPUBindGroupDescriptor bg_desc{};
        bg_desc.heap             = bind_group_heap;
        bg_desc.layout           = bind_group_layout;
        bg_desc.entries          = Span<GPUBindGroupEntry>(bg_entries);
        default_white_bind_group = device.create_bind_group(bg_desc);

        // 5. shader modules
        auto                      vcode = module->get_shader_blob("vsmain");
        GPUShaderModuleDescriptor vdesc{};
        vdesc.label  = "preview_vshader";
        vdesc.data   = vcode->data;
        vdesc.size   = vcode->size;
        auto vshader = device.create_shader_module(vdesc);

        auto                      fcode = module->get_shader_blob("fsmain");
        GPUShaderModuleDescriptor fdesc{};
        fdesc.label  = "preview_fshader";
        fdesc.data   = fcode->data;
        fdesc.size   = fcode->size;
        auto fshader = device.create_shader_module(fdesc);

        // 6. render pipeline
        GPURenderPipelineDescriptor pdesc{};
        pdesc.label  = "preview_render_pipeline";
        pdesc.layout = pipeline_layout;

        pdesc.vertex.module      = vshader;
        pdesc.vertex.entry_point = "vsmain";

        GPUVertexBufferLayout vbl{};
        vbl.array_stride = sizeof(PreviewVertex);
        vbl.step_mode    = GPUVertexStepMode::VERTEX;

        Vector<GPUVertexAttribute> attrs = {
            GPUVertexAttribute{GPUVertexFormat::FLOAT32x3, offsetof(PreviewVertex, position), 0, "POSITION"},
            GPUVertexAttribute{GPUVertexFormat::FLOAT32x3, offsetof(PreviewVertex, normal), 1, "NORMAL"},
            GPUVertexAttribute{GPUVertexFormat::FLOAT32x2, offsetof(PreviewVertex, uv), 2, "TEXCOORD"}};
        vbl.attributes = Span<GPUVertexAttribute>(attrs);

        Vector<GPUVertexBufferLayout> vbls = {vbl};
        pdesc.vertex.buffers               = Span<GPUVertexBufferLayout>(vbls);

        pdesc.fragment.module      = fshader;
        pdesc.fragment.entry_point = "fsmain";

        GPUColorTargetState cts{};
        cts.format     = GPUTextureFormat::RGBA8UNORM;
        cts.write_mask = GPUColorWrite::ALL;

        Vector<GPUColorTargetState> targets = {cts};
        pdesc.fragment.targets              = Span<GPUColorTargetState>(targets);

        pdesc.primitive.topology           = GPUPrimitiveTopology::TRIANGLE_LIST;
        pdesc.primitive.cull_mode          = GPUCullMode::NONE;
        pdesc.primitive.front_face         = GPUFrontFace::CCW;
        pdesc.primitive.strip_index_format = GPUIndexFormat::UINT32;

        pdesc.depth_stencil.depth_compare       = GPUCompareFunction::LESS_EQUAL;
        pdesc.depth_stencil.depth_write_enabled = true;
        pdesc.depth_stencil.format              = GPUTextureFormat::DEPTH32FLOAT;

        pdesc.multisample.count                     = 1;
        pdesc.multisample.alpha_to_coverage_enabled = false;

        pipeline = device.create_render_pipeline(pdesc);

        // 7. atlas offscreen textures
        GPUTextureDescriptor color_desc{};
        color_desc.label           = "preview_atlas_color";
        color_desc.format          = GPUTextureFormat::RGBA8UNORM;
        color_desc.dimension       = GPUTextureDimension::x2D;
        color_desc.size            = GPUExtent3D{atlas_w, atlas_h, 1};
        color_desc.array_layers    = 1;
        color_desc.mip_level_count = 1;
        color_desc.sample_count    = 1;
        color_desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::COPY_SRC;
        atlas_color                = device.create_texture(color_desc);
        atlas_color_view           = atlas_color.create_view();

        GPUTextureDescriptor depth_desc{};
        depth_desc.label           = "preview_atlas_depth";
        depth_desc.format          = GPUTextureFormat::DEPTH32FLOAT;
        depth_desc.dimension       = GPUTextureDimension::x2D;
        depth_desc.size            = GPUExtent3D{atlas_w, atlas_h, 1};
        depth_desc.array_layers    = 1;
        depth_desc.mip_level_count = 1;
        depth_desc.sample_count    = 1;
        depth_desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT;
        atlas_depth                = device.create_texture(depth_desc);
        atlas_depth_view           = atlas_depth.create_view();

        GPUBufferDescriptor readback_desc{};
        readback_desc.label = "preview_readback_buffer";
        readback_desc.size  = static_cast<GPUSize64>(atlas_w) * atlas_h * 4;
        readback_desc.usage = GPUBufferUsage::COPY_DST | GPUBufferUsage::MAP_READ;
        readback_buffer     = device.create_buffer(readback_desc);

        // 8. geometry megabuffers
        vbo_capacity = 32 * 1024 * 1024; // 32 MB
        ibo_capacity = 32 * 1024 * 1024; // 32 MB

        GPUBufferDescriptor vb_desc{};
        vb_desc.label = "preview_vbo";
        vb_desc.size  = vbo_capacity;
        vb_desc.usage = GPUBufferUsage::VERTEX | GPUBufferUsage::COPY_DST;
        vbo           = device.create_buffer(vb_desc);

        GPUBufferDescriptor vbs_desc{};
        vbs_desc.label = "preview_vbo_staging";
        vbs_desc.size  = vbo_capacity;
        vbs_desc.usage = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
        vbo_staging    = device.create_buffer(vbs_desc);

        GPUBufferDescriptor ib_desc{};
        ib_desc.label = "preview_ibo";
        ib_desc.size  = ibo_capacity;
        ib_desc.usage = GPUBufferUsage::INDEX | GPUBufferUsage::COPY_DST;
        ibo           = device.create_buffer(ib_desc);

        GPUBufferDescriptor ibs_desc{};
        ibs_desc.label = "preview_ibo_staging";
        ibs_desc.size  = ibo_capacity;
        ibs_desc.usage = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
        ibo_staging    = device.create_buffer(ibs_desc);

        gpu_initialized = true;
        return true;
    } catch (const std::exception& e) {
        get_logger()->warn("Exception during GPU preview initialization: {}, GPU preview disabled", e.what());
        gpu_failed = true;
        return false;
    } catch (...) {
        get_logger()->warn("Unknown exception during GPU preview initialization, GPU preview disabled");
        gpu_failed = true;
        return false;
    }
}

void BatchPreviewDispatcher::destroy_gpu_resources()
{
    if (!gpu_initialized) return;

    if (RHI::api()) {
        auto& device = RHI::get_current_device();
        device.wait();

        if (default_white_view.handle.valid()) default_white_view.destroy();
        if (default_white_texture.handle.valid()) default_white_texture.destroy();
        if (default_sampler.handle.valid()) default_sampler.destroy();
        if (bind_group_heap.handle.valid()) bind_group_heap.destroy();
        if (bind_group_layout.handle.valid()) bind_group_layout.destroy();

        if (readback_buffer.handle.valid()) readback_buffer.destroy();
        if (vbo_staging.handle.valid()) vbo_staging.destroy();
        if (ibo_staging.handle.valid()) ibo_staging.destroy();
        if (vbo.handle.valid()) vbo.destroy();
        if (ibo.handle.valid()) ibo.destroy();
        if (atlas_depth_view.handle.valid()) atlas_depth_view.destroy();
        if (atlas_depth.handle.valid()) atlas_depth.destroy();
        if (atlas_color_view.handle.valid()) atlas_color_view.destroy();
        if (atlas_color.handle.valid()) atlas_color.destroy();
        if (pipeline.handle.valid()) pipeline.destroy();
        if (pipeline_layout.handle.valid()) pipeline_layout.destroy();
    }

    owned_compiler.reset();
    compiler = {};

    gpu_initialized = false;
    gpu_failed      = false;
}

void BatchPreviewDispatcher::dispatch_batch_locked()
{
    if (pending_queue.empty()) return;

    Vector<PreviewRequest> batch;
    batch.swap(pending_queue);

    // fallback if GPU not available (e.g. MockGPU test harness)
    if (!init_gpu_resources()) {
        static const uint8_t dummy_pixel[4] = {0, 0, 0, 0};
        for (auto& req : batch) {
            stbi_write_png(req.thumb_path.string().c_str(), 1, 1, 4, dummy_pixel, 4);
            req.promise->set_value(req.rel_path);
        }
        return;
    }

    // 1. tile layout calculation
    uint thumb_w  = G_CONFIG.width;
    uint thumb_h  = G_CONFIG.height;
    uint ssaa     = G_CONFIG.enable_ssaa ? 2 : 1;
    uint render_w = thumb_w * ssaa;
    uint render_h = thumb_h * ssaa;
    uint cols     = std::max(1u, atlas_w / render_w);
    uint rows     = std::max(1u, atlas_h / render_h);

    Vector<PreviewVertex> packed_vertices;
    Vector<uint>          packed_indices;
    Vector<DrawBatchItem> draw_items;

    struct TransientTextureInfo
    {
        GPUTexture     texture;
        GPUTextureView view;
        GPUBuffer      staging;
    };
    Vector<TransientTextureInfo> transient_textures;

    auto& device = RHI::get_current_device();

    // 2. prepare command buffer early so texture uploads can be queued
    auto cmd_buf = device.create_command_buffer(GPUCommandBufferDescriptor{});

    for (usize i = 0; i < batch.size(); ++i) {
        auto& req = batch[i];

        auto [aabb_min, aabb_max] = req.scene.calculate_bounds();
        if (aabb_min.x > aabb_max.x) {
            continue;
        }

        Vector3 center     = (aabb_min + aabb_max) * 0.5f;
        Vector3 extents    = aabb_max - aabb_min;
        float   max_extent = std::max({extents.x, extents.y, extents.z});
        if (max_extent < 1e-6f) max_extent = 1.0f;
        float norm_scale = 2.0f / max_extent;

        float   fov_rad = glm::radians(G_CONFIG.fov_deg);
        float   dist    = (1.732f / std::tan(fov_rad * 0.5f)) * 1.15f;
        float   pitch   = glm::radians(G_CONFIG.pitch_deg);
        float   yaw     = glm::radians(G_CONFIG.yaw_deg);
        Vector3 eye(
            dist * std::cos(pitch) * std::sin(yaw),
            dist * std::sin(pitch),
            dist * std::cos(pitch) * std::cos(yaw));

        Matrix4x4 view = glm::lookAt(eye, Vector3(0.0f), Vector3(0.0f, 1.0f, 0.0f));
        Matrix4x4 proj = glm::perspective(fov_rad, static_cast<float>(render_w) / static_cast<float>(render_h), 0.1f, 100.0f);
        Matrix4x4 vp   = proj * view;

        uint tile_col = static_cast<uint>(i) % cols;
        uint tile_row = static_cast<uint>(i) / cols;
        uint tile_x   = tile_col * render_w;
        uint tile_y   = tile_row * render_h;

        // upload scene textures and create bind groups
        TreeMap<int, GPUBindGroup> texture_bind_groups;
        for (usize t = 0; t < req.scene.textures.size(); ++t) {
            const auto& ptex = req.scene.textures[t];
            if (ptex.width == 0 || ptex.height == 0 || ptex.pixels.empty()) continue;

            // convert 3-channel RGB to 4-channel RGBA if necessary
            Vector<uint8_t> rgba_data;
            const uint8_t*  src_rgba = ptex.pixels.data();
            if (ptex.channels == 3) {
                rgba_data.resize(static_cast<usize>(ptex.width) * ptex.height * 4);
                for (usize p = 0; p < static_cast<usize>(ptex.width) * ptex.height; ++p) {
                    rgba_data[p * 4 + 0] = ptex.pixels[p * 3 + 0];
                    rgba_data[p * 4 + 1] = ptex.pixels[p * 3 + 1];
                    rgba_data[p * 4 + 2] = ptex.pixels[p * 3 + 2];
                    rgba_data[p * 4 + 3] = 255;
                }
                src_rgba = rgba_data.data();
            }

            // downscale large textures to max 256x256 for rapid upload & preview fidelity
            uint            upload_w = ptex.width;
            uint            upload_h = ptex.height;
            Vector<uint8_t> downscaled;
            constexpr uint  MAX_TEX_DIM = 256;
            if (upload_w > MAX_TEX_DIM || upload_h > MAX_TEX_DIM) {
                if (upload_w > upload_h) {
                    upload_h = std::max(1u, static_cast<uint>(static_cast<float>(upload_h) / upload_w * MAX_TEX_DIM));
                    upload_w = MAX_TEX_DIM;
                } else {
                    upload_w = std::max(1u, static_cast<uint>(static_cast<float>(upload_w) / upload_h * MAX_TEX_DIM));
                    upload_h = MAX_TEX_DIM;
                }
                downscaled.resize(static_cast<usize>(upload_w) * upload_h * 4);
                stbir_resize_uint8_linear(
                    src_rgba, ptex.width, ptex.height, 0,
                    downscaled.data(), upload_w, upload_h, 0,
                    STBIR_RGBA);
                src_rgba = downscaled.data();
            }

            // allocate GPU texture
            GPUTextureDescriptor tdesc{};
            tdesc.label           = "preview_material_tex";
            tdesc.format          = GPUTextureFormat::RGBA8UNORM;
            tdesc.dimension       = GPUTextureDimension::x2D;
            tdesc.size            = GPUExtent3D{upload_w, upload_h, 1};
            tdesc.array_layers    = 1;
            tdesc.mip_level_count = 1;
            tdesc.sample_count    = 1;
            tdesc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
            auto tex_obj          = device.create_texture(tdesc);
            auto tex_view         = tex_obj.create_view();

            // allocate staging buffer with 256-byte aligned pitch
            uint  row_pitch  = (upload_w * 4 + 255) & ~255;
            usize stage_size = static_cast<usize>(row_pitch) * upload_h;

            GPUBufferDescriptor s_desc{};
            s_desc.size    = stage_size;
            s_desc.usage   = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
            auto stage_buf = device.create_buffer(s_desc);
            stage_buf.map(GPUMapMode::WRITE, 0, stage_size);
            auto mapped = stage_buf.get_mapped_range<uint8_t>();
            for (uint r = 0; r < upload_h; ++r) {
                std::memcpy(mapped.data + r * row_pitch, src_rgba + r * upload_w * 4, upload_w * 4);
            }
            stage_buf.unmap();

            // copy staging to texture
            cmd_buf.resource_barrier(state_transition(tex_obj, undefined_texture_state(), copy_dst_texture_state()));
            GPUTexelCopyBufferInfo cp_src{};
            cp_src.buffer         = stage_buf;
            cp_src.bytes_per_row  = row_pitch;
            cp_src.rows_per_image = upload_h;

            GPUTexelCopyTextureInfo cp_dst{};
            cp_dst.texture = tex_obj;
            cp_dst.aspect  = GPUTextureAspect::COLOR;
            cmd_buf.copy_buffer_to_texture(cp_src, cp_dst, GPUExtent3D{upload_w, upload_h, 1});
            cmd_buf.resource_barrier(state_transition(tex_obj, copy_dst_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));

            // create bind group for this texture
            Array<GPUBindGroupEntry, 2> bg_entries{};
            bg_entries[0].type    = GPUResourceType::TEXTURE;
            bg_entries[0].binding = 0;
            bg_entries[0].texture = tex_view;

            bg_entries[1].type    = GPUResourceType::SAMPLER;
            bg_entries[1].binding = 1;
            bg_entries[1].sampler = default_sampler;

            GPUBindGroupDescriptor bg_desc{};
            bg_desc.heap    = bind_group_heap;
            bg_desc.layout  = bind_group_layout;
            bg_desc.entries = Span<GPUBindGroupEntry>(bg_entries);
            auto bg         = device.create_bind_group(bg_desc);

            texture_bind_groups[static_cast<int>(t)] = bg;
            transient_textures.push_back({tex_obj, tex_view, stage_buf});
        }

        for (const auto& mesh : req.scene.meshes) {
            if (mesh.positions.empty()) continue;

            uint base_vertex = static_cast<uint>(packed_vertices.size());
            uint first_index = static_cast<uint>(packed_indices.size());

            Vector4      base_color = Vector4(1.0f);
            GPUBindGroup bind_group = default_white_bind_group;

            if (mesh.material_id >= 0 && static_cast<usize>(mesh.material_id) < req.scene.materials.size()) {
                const auto& mat = req.scene.materials[mesh.material_id];
                base_color      = mat.base_color_factor;
                if (mat.albedo_texture_id >= 0) {
                    auto it = texture_bind_groups.find(mat.albedo_texture_id);
                    if (it != texture_bind_groups.end()) {
                        bind_group = it->second;
                    }
                }
            }

            Matrix3x3 norm_matrix = glm::transpose(glm::inverse(Matrix3x3(mesh.transform)));

            for (usize v = 0; v < mesh.positions.size(); ++v) {
                Vector4 world_p = mesh.transform * Vector4(mesh.positions[v], 1.0f);
                Vector3 wp      = Vector3(world_p) / (world_p.w != 0.0f ? world_p.w : 1.0f);
                Vector3 p       = (wp - center) * norm_scale;

                Vector3 n = (v < mesh.normals.size())
                                ? glm::normalize(norm_matrix * mesh.normals[v])
                                : Vector3(0.0f, 1.0f, 0.0f);
                Vector2 u = (v < mesh.uvs.size()) ? mesh.uvs[v] : Vector2(0.0f);
                packed_vertices.push_back(PreviewVertex{p, n, u});
            }

            if (!mesh.indices.empty()) {
                for (auto idx : mesh.indices) {
                    packed_indices.push_back(idx);
                }
            } else {
                for (uint idx = 0; idx < static_cast<uint>(mesh.positions.size()); ++idx) {
                    packed_indices.push_back(idx);
                }
            }

            DrawBatchItem item{};
            item.index_count  = static_cast<uint>(packed_indices.size() - first_index);
            item.first_index  = first_index;
            item.base_vertex  = static_cast<int>(base_vertex);
            item.vp           = vp;
            item.base_color   = base_color;
            item.view_dir_pad = Vector4(glm::normalize(eye), 0.0f);
            item.bind_group   = bind_group;
            item.tile_x       = tile_x;
            item.tile_y       = tile_y;
            item.tile_w       = render_w;
            item.tile_h       = render_h;
            draw_items.push_back(item);
        }
    }

    if (draw_items.empty() || packed_vertices.empty()) {
        cmd_buf.submit();
        device.wait();
        for (auto& t : transient_textures) {
            if (t.view.handle.valid()) t.view.destroy();
            if (t.texture.handle.valid()) t.texture.destroy();
            if (t.staging.handle.valid()) t.staging.destroy();
        }

        static const uint8_t dummy_pixel[4] = {0, 0, 0, 0};
        for (auto& req : batch) {
            stbi_write_png(req.thumb_path.string().c_str(), 1, 1, 4, dummy_pixel, 4);
            req.promise->set_value(req.rel_path);
        }
        return;
    }

    usize vbo_bytes = packed_vertices.size() * sizeof(PreviewVertex);
    usize ibo_bytes = packed_indices.size() * sizeof(uint);

    // dynamically resize geometry buffers if needed
    if (vbo_bytes > vbo_capacity || ibo_bytes > ibo_capacity) {
        device.wait();

        if (vbo_bytes > vbo_capacity) {
            vbo_capacity = std::max(vbo_capacity * 2, vbo_bytes);
            if (vbo.handle.valid()) vbo.destroy();
            if (vbo_staging.handle.valid()) vbo_staging.destroy();

            GPUBufferDescriptor vb_desc{};
            vb_desc.label = "preview_vbo";
            vb_desc.size  = vbo_capacity;
            vb_desc.usage = GPUBufferUsage::VERTEX | GPUBufferUsage::COPY_DST;
            vbo           = device.create_buffer(vb_desc);

            GPUBufferDescriptor vbs_desc{};
            vbs_desc.label = "preview_vbo_staging";
            vbs_desc.size  = vbo_capacity;
            vbs_desc.usage = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
            vbo_staging    = device.create_buffer(vbs_desc);
        }

        if (ibo_bytes > ibo_capacity) {
            ibo_capacity = std::max(ibo_capacity * 2, ibo_bytes);
            if (ibo.handle.valid()) ibo.destroy();
            if (ibo_staging.handle.valid()) ibo_staging.destroy();

            GPUBufferDescriptor ib_desc{};
            ib_desc.label = "preview_ibo";
            ib_desc.size  = ibo_capacity;
            ib_desc.usage = GPUBufferUsage::INDEX | GPUBufferUsage::COPY_DST;
            ibo           = device.create_buffer(ib_desc);

            GPUBufferDescriptor ibs_desc{};
            ibs_desc.label = "preview_ibo_staging";
            ibs_desc.size  = ibo_capacity;
            ibs_desc.usage = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
            ibo_staging    = device.create_buffer(ibs_desc);
        }
    }

    // 3. upload geometry to staging
    vbo_staging.map(GPUMapMode::WRITE, 0, vbo_bytes);
    auto vbo_range = vbo_staging.get_mapped_range<PreviewVertex>();
    std::memcpy(vbo_range.data, packed_vertices.data(), vbo_bytes);
    vbo_staging.unmap();

    ibo_staging.map(GPUMapMode::WRITE, 0, ibo_bytes);
    auto ibo_range = ibo_staging.get_mapped_range<uint>();
    std::memcpy(ibo_range.data, packed_indices.data(), ibo_bytes);
    ibo_staging.unmap();

    cmd_buf.copy_buffer_to_buffer(vbo_staging, 0, vbo, 0, vbo_bytes);
    cmd_buf.copy_buffer_to_buffer(ibo_staging, 0, ibo, 0, ibo_bytes);

    cmd_buf.resource_barrier(state_transition(atlas_color, undefined_texture_state(), color_attachment_texture_state()));
    cmd_buf.resource_barrier(state_transition(atlas_depth, undefined_texture_state(), depth_stencil_attachment_texture_state()));

    GPURenderPassColorAttachment cat{};
    cat.view        = atlas_color_view;
    cat.load_op     = GPULoadOp::CLEAR;
    cat.store_op    = GPUStoreOp::STORE;
    cat.clear_value = GPUColor{G_CONFIG.clear_color.r, G_CONFIG.clear_color.g, G_CONFIG.clear_color.b, G_CONFIG.clear_color.a};

    GPURenderPassDepthStencilAttachment dat{};
    dat.view              = atlas_depth_view;
    dat.depth_load_op     = GPULoadOp::CLEAR;
    dat.depth_store_op    = GPUStoreOp::STORE;
    dat.depth_clear_value = 1.0f;

    GPURenderPassDescriptor rp_desc{};
    rp_desc.label                                          = "preview_atlas_pass";
    Vector<GPURenderPassColorAttachment> color_attachments = {cat};
    rp_desc.color_attachments                              = Span<GPURenderPassColorAttachment>(color_attachments);
    rp_desc.depth_stencil_attachment                       = dat;

    cmd_buf.begin_render_pass(rp_desc);
    cmd_buf.set_pipeline(pipeline);
    cmd_buf.set_vertex_buffer(0, vbo, 0, vbo_bytes);
    cmd_buf.set_index_buffer(ibo, GPUIndexFormat::UINT32, 0, ibo_bytes);

    for (const auto& item : draw_items) {
        cmd_buf.set_viewport(static_cast<float>(item.tile_x), static_cast<float>(item.tile_y), static_cast<float>(item.tile_w), static_cast<float>(item.tile_h), 0.0f, 1.0f);
        cmd_buf.set_scissor_rect(item.tile_x, item.tile_y, item.tile_w, item.tile_h);

        cmd_buf.set_bind_group(0, item.bind_group);

        PreviewDrawConstants pc{};
        pc.vp           = item.vp;
        pc.base_color   = item.base_color;
        pc.view_dir_pad = item.view_dir_pad;
        cmd_buf.set_immediates(0, sizeof(PreviewDrawConstants), &pc);

        cmd_buf.draw_indexed(item.index_count, 1, item.first_index, item.base_vertex, 0);
    }

    cmd_buf.end_render_pass();

    // transition atlas color from attachment to transfer source
    cmd_buf.resource_barrier(state_transition(atlas_color, color_attachment_texture_state(), copy_src_texture_state()));

    // 4. copy atlas to readback buffer
    GPUTexelCopyTextureInfo src_copy{};
    src_copy.texture = atlas_color;
    src_copy.aspect  = GPUTextureAspect::COLOR;

    GPUTexelCopyBufferInfo dst_copy{};
    dst_copy.buffer         = readback_buffer;
    dst_copy.bytes_per_row  = atlas_w * 4;
    dst_copy.rows_per_image = atlas_h;

    GPUExtent3D copy_size{atlas_w, atlas_h, 1};
    cmd_buf.copy_texture_to_buffer(src_copy, dst_copy, copy_size);

    cmd_buf.submit();

    // 5. wait for completion and map readback buffer
    device.wait();

    // destroy per-batch transient textures and staging buffers
    for (auto& t : transient_textures) {
        if (t.view.handle.valid()) t.view.destroy();
        if (t.texture.handle.valid()) t.texture.destroy();
        if (t.staging.handle.valid()) t.staging.destroy();
    }

    readback_buffer.map(GPUMapMode::READ, 0, static_cast<GPUSize64>(atlas_w) * atlas_h * 4);
    auto           mapped_range   = readback_buffer.get_mapped_range<uint8_t>();
    const uint8_t* raw_atlas_data = mapped_range.data;

    // 6. parallel tile crop, SSAA downscale and PNG write
    auto atlas_copy = make_ref<Vector<uint8_t>>(atlas_w * atlas_h * 4);
    std::memcpy(atlas_copy->data(), raw_atlas_data, atlas_copy->size());
    readback_buffer.unmap();

    auto pending_jobs = make_ref<Atomic<usize>>(batch.size());
    auto wait_mutex   = make_ref<Mutex>();
    auto wait_cv      = make_ref<ConditionVariable>();

    for (usize i = 0; i < batch.size(); ++i) {
        JobScheduler::schedule(JobPriority::BACKGROUND, [this, i, cols, render_w, render_h, thumb_w, thumb_h, ssaa, atlas_copy, req = batch[i], pending_jobs, wait_mutex, wait_cv]() {
            uint tile_col = static_cast<uint>(i) % cols;
            uint tile_row = static_cast<uint>(i) / cols;
            uint tile_x   = tile_col * render_w;
            uint tile_y   = tile_row * render_h;

            Vector<uint8_t> tile_raw(render_w * render_h * 4);
            for (uint row = 0; row < render_h; ++row) {
                const uint8_t* src_row = atlas_copy->data() + ((tile_y + row) * atlas_w + tile_x) * 4;
                uint8_t*       dst_row = tile_raw.data() + (row * render_w) * 4;
                std::memcpy(dst_row, src_row, render_w * 4);
            }

            Vector<uint8_t> final_pixels;
            if (ssaa > 1) {
                final_pixels.resize(thumb_w * thumb_h * 4);
                stbir_resize_uint8_linear(
                    tile_raw.data(), render_w, render_h, 0,
                    final_pixels.data(), thumb_w, thumb_h, 0,
                    STBIR_RGBA);
            } else {
                final_pixels = std::move(tile_raw);
            }

            if (!stbi_write_png(req.thumb_path.string().c_str(), thumb_w, thumb_h, 4, final_pixels.data(), thumb_w * 4)) {
                get_logger()->warn("Failed to write preview thumbnail: {}", req.thumb_path.string());
            }

            req.promise->set_value(req.rel_path);

            if (pending_jobs->fetch_sub(1) == 1) {
                UniqueLock lk(*wait_mutex);
                wait_cv->notify_all();
            }
        });
    }

    // wait for all tile writes to complete before returning
    {
        UniqueLock lk(*wait_mutex);
        wait_cv->wait(lk, [&]() {
            return pending_jobs->load() == 0;
        });
    }
}
