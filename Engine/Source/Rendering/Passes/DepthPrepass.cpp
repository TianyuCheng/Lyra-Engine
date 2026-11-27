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

    pipeline_manager.init(device, compiler);

    // compile and reflect depth-only shader asset
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module         = (shaders_root / "Forward/DepthPrepass.slang").string();
    shader_desc.vertex_entry   = "vsmain";
    shader_desc.fragment_entry = "fsmain";

    auto layout_info = pipeline_manager.reflect_render_layout(shader_desc);
    if (!layout_info.valid()) {
        return false;
    }
    blayouts.assign(layout_info.bind_group_layouts.begin(), layout_info.bind_group_layouts.end());

    GPUDepthStencilState ds{};
    ds.depth_compare       = GPUCompareFunction::LESS_EQUAL;
    ds.depth_write_enabled = true;
    ds.format              = cached_depth_format;

    try {
        pipeline = pipeline_manager.render_pipeline_builder()
                       .with_shader(shader_desc)
                       .with_cull_mode(GPUCullMode::BACK)
                       .with_topology(GPUPrimitiveTopology::TRIANGLE_LIST)
                       .with_front_face(GPUFrontFace::CCW)
                       .with_depth_stencil(ds)
                       .with_label("depth_prepass_pipeline")
                       .build();
    } catch (const std::exception&) {
        return false;
    }

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

    pipeline_manager.destroy();
    pipeline = {};
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
