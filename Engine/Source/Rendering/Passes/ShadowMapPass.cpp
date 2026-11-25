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

    // 1. compile vertex and fragment shaders from sys://Shaders/Forward/ShadowDepth.slang
    auto shader_path = (shaders_root / "Forward/ShadowDepth.slang").string();
    auto module      = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "shadow_depth";
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
        desc.label = "shadow_depth_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "shadow_depth_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    if (!vshader.valid() || !fshader.valid()) {
        return false;
    }

    // 2. create bind group layouts and pipeline layout
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

    // 3. create render pipeline
    pipeline = execute([&]() {
        auto desc                                 = GPURenderPipelineDescriptor{};
        desc.layout                               = playout;
        desc.primitive.cull_mode                  = GPUCullMode::BACK;
        desc.primitive.topology                   = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face                 = GPUFrontFace::CCW;
        desc.primitive.strip_index_format         = GPUIndexFormat::UINT32;
        desc.depth_stencil.depth_compare          = GPUCompareFunction::LESS_EQUAL;
        desc.depth_stencil.depth_write_enabled    = true;
        desc.depth_stencil.format                 = cached_depth_format;
        desc.depth_stencil.depth_bias             = 2;
        desc.depth_stencil.depth_bias_slope_scale = 1.75f;
        desc.multisample.count                    = 1;
        desc.vertex.module                        = vshader;
        desc.vertex.buffers                       = {}; // vertex pulling: no input assembler buffers
        desc.vertex.entry_point                   = "vsmain";
        desc.fragment.module                      = fshader;
        desc.fragment.entry_point                 = "fsmain";
        desc.fragment.targets                     = {}; // depth only pass

        return device.create_render_pipeline(desc);
    });

    if (!pipeline.valid()) {
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

    if (pipeline.valid()) {
        pipeline.destroy();
    }
    if (playout.valid()) {
        playout.destroy();
    }
    if (fshader.valid()) {
        fshader.destroy();
    }
    if (vshader.valid()) {
        vshader.destroy();
    }
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
