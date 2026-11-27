#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/ShadowMapPass.h>

using namespace lyra;
using namespace lyra::passes;

bool ShadowMapPass::init(
    GPUDevice                   dev,
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

    device              = dev;
    cached_depth_format = depth_format;

    pipeline_manager.init(device, compiler);

    // 1. compile and reflect vertex and fragment shaders from sys://Shaders/Forward/ShadowDepth.slang
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module         = (shaders_root / "Forward/ShadowDepth.slang").string();
    shader_desc.vertex_entry   = "vsmain";
    shader_desc.fragment_entry = "fsmain";

    auto layout_info = pipeline_manager.reflect_render_layout(shader_desc);
    if (!layout_info.valid()) {
        return false;
    }
    blayouts.assign(layout_info.bind_group_layouts.begin(), layout_info.bind_group_layouts.end());

    GPUDepthStencilState ds{};
    ds.depth_compare          = GPUCompareFunction::LESS_EQUAL;
    ds.depth_write_enabled    = true;
    ds.format                 = cached_depth_format;
    ds.depth_bias             = 2;
    ds.depth_bias_slope_scale = 1.75f;

    try {
        pipeline = pipeline_manager.render_pipeline_builder()
                       .with_shader(shader_desc)
                       .with_cull_mode(GPUCullMode::BACK)
                       .with_topology(GPUPrimitiveTopology::TRIANGLE_LIST)
                       .with_front_face(GPUFrontFace::CCW)
                       .with_depth_stencil(ds)
                       .with_label("shadow_depth_pipeline")
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

void ShadowMapPass::destroy()
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

ShadowMapData ShadowMapPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    uint               cascade_count,
    uint               resolution)
{
    // calculate directional light view-proj
    Vector3 light_dir = scene.directional_lights.empty()
                            ? glm::normalize(Vector3(0.6f, -1.0f, -0.5f))
                            : glm::normalize(scene.directional_lights[0].direction);

    Vector3   scene_center = Vector3(0.0f, 0.5f, 0.0f);
    Vector3   light_pos    = scene_center - light_dir * 30.0f;
    Matrix4x4 l_view       = glm::lookAt(light_pos, scene_center, Vector3(0.0f, 1.0f, 0.0f));
    Matrix4x4 l_proj       = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 60.0f);
    light_vp               = l_proj * l_view;

    ShadowCascadeData cascade_data{};
    cascade_data.light_view_proj = light_vp;

    return builder.add_pass<ShadowMapData>(
        "ShadowMapPass",
        [&](ShadowMapData& data, FrameGraphPassBuilder& pass_builder) {
            auto desc             = GPUTextureDescriptor{};
            desc.label            = "shadow_map_cascades";
            desc.size.width       = resolution;
            desc.size.height      = resolution;
            desc.size.depth       = 1;
            desc.array_layers     = cascade_count;
            desc.dimension        = GPUTextureDimension::x2D;
            desc.format           = cached_depth_format;
            desc.usage            = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.shadow_map_array = pass_builder.create<FrameGraphTexture>(desc);
            data.shadow_map_array = pass_builder.render(data.shadow_map_array);
        },
        [this, cascade_data, resolution, &scene](const ShadowMapData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto shadow_tex = resources.get<FrameGraphTexture>(data.shadow_map_array);
            if (!shadow_tex) {
                return;
            }

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = shadow_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto desc                     = GPURenderPassDescriptor{};
            desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(resolution), static_cast<float>(resolution));
            command.set_scissor_rect(0, 0, resolution, resolution);
            command.set_pipeline(pipeline);

            if (!blayouts.empty()) {
                auto u_slice = ctx->create_uniform(cascade_data);

                Array<GPUBindGroupEntry, 1> entries{};
                entries[0].type    = GPUResourceType::BUFFER;
                entries[0].binding = 0;
                entries[0].buffer  = u_slice;

                auto bg_desc    = GPUBindGroupDescriptor{};
                bg_desc.layout  = blayouts[0];
                bg_desc.entries = entries;
                auto bg         = ctx->allocate(bg_desc);
                if (bg.valid()) {
                    command.set_bind_group(0, bg);
                }
            }

            uint64_t last_bound_vbuf = ~0ull;

            // rasterize all casting scene meshes
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
