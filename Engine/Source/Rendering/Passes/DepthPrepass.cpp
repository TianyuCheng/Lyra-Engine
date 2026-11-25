#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/DepthPrepass.h>
#include <Lyra/Rendering/ViewUniforms.h>

using namespace lyra;
using namespace lyra::passes;

bool DepthPrepass::init(
    GPUDevice                   device,
    Compiler                    compiler,
    GPUTextureFormat            depth_format,
    const Path&                 shaders_root,
    Ref<GeometryBindGroupCache> cache)
{
    if (initialized) {
        if (cached_depth_format == depth_format) {
            return true;
        }
        destroy();
    }

    this->device              = device;
    this->cached_depth_format = depth_format;

    // compile depth-only shader asset
    auto shader_path = (shaders_root / "Forward/DepthPrepass.slang").string();
    auto module      = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "depth_prepass";
        desc.path   = shader_path.c_str();
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
        desc.label = "depth_prepass_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "depth_prepass_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    blayouts.clear();
    for (auto& desc : reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        blayouts.push_back(blayout);
    }

    playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = blayouts;
        desc.immediate_size     = reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    pipeline = execute([&]() {
        auto desc                              = GPURenderPipelineDescriptor{};
        desc.layout                            = playout;
        desc.primitive.cull_mode               = GPUCullMode::BACK;
        desc.primitive.topology                = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face              = GPUFrontFace::CCW;
        desc.primitive.strip_index_format      = GPUIndexFormat::UINT32;
        desc.depth_stencil.depth_compare       = GPUCompareFunction::LESS_EQUAL;
        desc.depth_stencil.depth_write_enabled = true;
        desc.depth_stencil.format              = cached_depth_format;
        desc.multisample.count                 = 1;
        desc.vertex.module                     = vshader;
        desc.vertex.buffers                    = {}; // vertex pulling: no input assembler buffers
        desc.vertex.entry_point                = "vsmain";
        desc.fragment.module                   = fshader;
        desc.fragment.targets                  = {}; // no color targets for depth-only pass
        desc.fragment.entry_point              = "fsmain";
        return device.create_render_pipeline(desc);
    });

    if (cache) {
        geometry_cache = cache;
        if (blayouts.size() > 1 && (!geometry_cache->get_layout().valid() || !geometry_cache->valid())) {
            geometry_cache->init(device, blayouts[1]);
        }
    } else {
        geometry_cache = make_ref<GeometryBindGroupCache>();
        geometry_cache->init(device, blayouts.size() > 1 ? blayouts[1] : GPUBindGroupLayoutHandle{});
    }

    initialized = pipeline.valid() && geometry_cache && geometry_cache->valid();
    return initialized;
}

void DepthPrepass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();
    blayouts.clear();
    geometry_cache.reset();

    initialized = false;
}

DepthPrepassData DepthPrepass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    GPUExtent2D        extent)
{
    (void)view;

    return builder.add_pass<DepthPrepassData>(
        "DepthPrepass",
        [&](DepthPrepassData& data, FrameGraphPassBuilder& pass_builder) {
            auto desc        = GPUTextureDescriptor{};
            desc.label       = "depth_prepass_buffer";
            desc.size.width  = extent.width;
            desc.size.height = extent.height;
            desc.size.depth  = 1;
            desc.dimension   = GPUTextureDimension::x2D;
            desc.format      = cached_depth_format;
            desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;

            data.depth_texture = pass_builder.create<FrameGraphTexture>(desc);
            data.depth_texture = pass_builder.render(data.depth_texture);
        },
        [this, extent, &scene](const DepthPrepassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_texture);
            if (!depth_tex) {
                return;
            }

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto desc                     = GPURenderPassDescriptor{};
            desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            if (ctx->view_uniforms && ctx->arena && !blayouts.empty()) {
                auto camera_bg = ctx->view_uniforms->bind_group(blayouts[0], *ctx->arena, ctx->device);
                if (camera_bg.valid()) {
                    command.set_bind_group(0, camera_bg);
                }
            }

            uint64_t last_bound_vbuf = ~0ull;

            // rasterize all opaque scene meshes
            for (const auto& mesh : scene.meshes) {
                if (!mesh.geometry.valid() || mesh.index_count == 0) {
                    continue;
                }

                GPUBufferHandle vb_handle = mesh.geometry.vertex_slice.buffer;
                if (!vb_handle.valid()) {
                    continue;
                }

                if (blayouts.size() > 1) {
                    auto bg = mesh.geometry.vertex_bind_group;
                    if (!bg.valid() && geometry_cache) {
                        bg = geometry_cache->get_or_create(vb_handle);
                    }

                    if (bg.valid() && last_bound_vbuf != vb_handle.value) {
                        command.set_bind_group(1, bg);
                        last_bound_vbuf = vb_handle.value;
                    }
                }

                const auto& offsets     = mesh.geometry.stream_offsets;
                GPUSize64   base_offset = mesh.geometry.vertex_slice.offset;

                InstanceData inst{};
                inst.model           = mesh.transform;
                inst.position_offset = static_cast<uint>(base_offset + (offsets.has_position() ? offsets.position : 0));

                command.set_immediates(0, inst);

                GPUBuffer ibuf{};
                ibuf.handle = mesh.geometry.index_slice.buffer;

                command.set_index_buffer(ibuf, mesh.geometry.index_format, mesh.geometry.index_slice.offset);
                command.draw_indexed(mesh.index_count, 1, mesh.first_index, 0, 0);
            }

            command.end_render_pass();
        });
}
