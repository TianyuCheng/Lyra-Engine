#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/ShadowMapPass.h>

using namespace lyra;
using namespace lyra::passes;

bool ShadowMapPass::init(
    GPUDevice        dev,
    Compiler         compiler,
    GPUTextureFormat depth_format)
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
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "shadow_depth";
        desc.path   = "sys://Shaders/Forward/ShadowDepth.slang";
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
        Vector<GPUVertexAttribute>    attributes;
        Vector<GPUVertexBufferLayout> vlayouts;

        auto pos_attr            = GPUVertexAttribute{};
        pos_attr.format          = GPUVertexFormat::FLOAT32x3;
        pos_attr.offset          = 0;
        pos_attr.shader_location = 0;
        pos_attr.shader_semantic = "POSITION";
        attributes.push_back(pos_attr);

        auto vlayout         = GPUVertexBufferLayout{};
        vlayout.array_stride = sizeof(Vector3); // position stream stride: 12 bytes
        vlayout.step_mode    = GPUVertexStepMode::VERTEX;
        vlayout.attributes   = attributes;
        vlayouts.push_back(vlayout);

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
        desc.vertex.buffers                       = vlayouts;
        desc.vertex.entry_point                   = "vsmain";
        desc.fragment.module                      = fshader;
        desc.fragment.entry_point                 = "fsmain";
        desc.fragment.targets                     = {}; // depth only pass

        return device.create_render_pipeline(desc);
    });

    if (!pipeline.valid()) {
        return false;
    }

    // 4. cascade uniform buffer and bind group
    cascade_uniform_buffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "shadow_cascade_uniform_buffer";
        desc.size               = sizeof(ShadowCascadeData);
        desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 512;
        return device.create_bind_group_heap(desc);
    });

    if (!blayouts.empty()) {
        bind_group = execute([&]() {
            Array<GPUBindGroupEntry, 1> entries{};
            entries[0].type          = GPUResourceType::BUFFER;
            entries[0].binding       = 0;
            entries[0].buffer.buffer = cascade_uniform_buffer.handle;
            entries[0].buffer.offset = 0;
            entries[0].buffer.size   = sizeof(ShadowCascadeData);

            auto desc    = GPUBindGroupDescriptor{};
            desc.heap    = heap;
            desc.layout  = blayouts[0];
            desc.entries = entries;
            return device.create_bind_group(desc);
        });
    }

    initialized = true;
    return true;
}

void ShadowMapPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) {
        heap.destroy();
    }
    if (cascade_uniform_buffer.valid()) {
        cascade_uniform_buffer.destroy();
    }
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
    bind_group  = {};
    initialized = false;
}

ShadowMapData ShadowMapPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    uint               cascade_count,
    uint               resolution)
{
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
        [this, resolution, &view, &scene](const ShadowMapData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto shadow_tex = resources.get<FrameGraphTexture>(data.shadow_map_array);
            if (!shadow_tex) {
                return;
            }

            // calculate directional light view-proj
            Vector3 light_dir = scene.directional_lights.empty()
                                    ? glm::normalize(Vector3(0.5f, -1.0f, 0.5f))
                                    : glm::normalize(scene.directional_lights[0].direction);

            Vector3   light_pos = view.camera_position - light_dir * 50.0f;
            Matrix4x4 l_view    = glm::lookAt(light_pos, view.camera_position, Vector3(0.0f, 1.0f, 0.0f));
            Matrix4x4 l_proj    = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, 0.1f, 100.0f);
            Matrix4x4 l_vp      = l_proj * l_view;

            if (cascade_uniform_buffer.valid()) {
                auto mapped = cascade_uniform_buffer.get_mapped_range<ShadowCascadeData>();
                if (mapped.count > 0) {
                    mapped.at(0).light_view_proj = l_vp;
                }
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
            if (bind_group.valid()) {
                command.set_bind_group(0, bind_group);
            }

            // rasterize all casting scene meshes
            for (const auto& mesh : scene.meshes) {
                if (!mesh.geometry.valid() || mesh.index_count == 0) {
                    continue;
                }

                InstanceData inst{mesh.transform};
                command.set_immediates(0, inst);

                GPUBuffer vbuf{};
                vbuf.handle = mesh.geometry.vertex_slice.buffer;
                GPUBuffer ibuf{};
                ibuf.handle = mesh.geometry.index_slice.buffer;

                const auto& offsets    = mesh.geometry.stream_offsets;
                GPUSize64   pos_offset = mesh.geometry.vertex_slice.offset + (offsets.has_position() ? offsets.position : 0);

                command.set_vertex_buffer(0, vbuf, pos_offset);
                command.set_index_buffer(ibuf, mesh.geometry.index_format, mesh.geometry.index_slice.offset);
                command.draw_indexed(mesh.index_count, 1, mesh.first_index, 0, 0);
            }

            command.end_render_pass();
        });
}
