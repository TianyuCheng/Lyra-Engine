#include <Lyra/Rendering/Passes/ForwardPass.h>
#include <Lyra/Graphics/RHIAPI.h>

using namespace lyra;
using namespace lyra::passes;

bool ForwardPass::init(
    GPUDevice        dev,
    Compiler         compiler,
    GPUTextureFormat color_format,
    GPUTextureFormat depth_format,
    const Path&      shaders_root)
{
    if (initialized) {
        if (cached_color_format == color_format && cached_depth_format == depth_format) {
            return true;
        }
        destroy();
    }

    device              = dev;
    cached_color_format = color_format;
    cached_depth_format = depth_format;

    // 1. compile vertex and fragment shaders from sys://Shaders/Forward/Forward.slang
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "forward";
        desc.path   = "sys://Shaders/Forward/Forward.slang";
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
        desc.label = "forward_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "forward_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    if (!vshader.valid() || !fshader.valid()) {
        return false;
    }

    // 2. bind group layouts and pipeline layout
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
        Vector<GPUColorTargetState>   targets;

        auto pos_attr            = GPUVertexAttribute{};
        pos_attr.format          = GPUVertexFormat::FLOAT32x3;
        pos_attr.offset          = 0;
        pos_attr.shader_location = 0;
        attributes.push_back(pos_attr);

        auto norm_attr            = GPUVertexAttribute{};
        norm_attr.format          = GPUVertexFormat::FLOAT32x3;
        norm_attr.offset          = 12;
        norm_attr.shader_location = 1;
        attributes.push_back(norm_attr);

        auto uv_attr            = GPUVertexAttribute{};
        uv_attr.format          = GPUVertexFormat::FLOAT32x2;
        uv_attr.offset          = 24;
        uv_attr.shader_location = 2;
        attributes.push_back(uv_attr);

        auto vlayout         = GPUVertexBufferLayout{};
        vlayout.array_stride = sizeof(Vector3) * 2 + sizeof(Vector2); // 32 bytes interleaved
        vlayout.step_mode    = GPUVertexStepMode::VERTEX;
        vlayout.attributes   = attributes;
        vlayouts.push_back(vlayout);

        auto color_target         = GPUColorTargetState{};
        color_target.format       = cached_color_format;
        color_target.blend_enable = false;
        targets.push_back(color_target);

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
        desc.vertex.buffers                    = vlayouts;
        desc.vertex.entry_point                = "vsmain";
        desc.fragment.module                   = fshader;
        desc.fragment.entry_point              = "fsmain";
        desc.fragment.targets                  = targets;

        return device.create_render_pipeline(desc);
    });

    if (!pipeline.valid()) {
        return false;
    }

    // 4. camera uniform buffer and bind group
    camera_buffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "forward_camera_uniform_buffer";
        desc.size               = sizeof(CameraUniforms);
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
        camera_bind_group = execute([&]() {
            Array<GPUBindGroupEntry, 1> entries{};
            entries[0].type          = GPUResourceType::BUFFER;
            entries[0].binding       = 0;
            entries[0].buffer.buffer = camera_buffer.handle;
            entries[0].buffer.offset = 0;
            entries[0].buffer.size   = sizeof(CameraUniforms);

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

void ForwardPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) {
        heap.destroy();
    }
    if (camera_buffer.valid()) {
        camera_buffer.destroy();
    }
    if (pipeline.valid()) {
        pipeline.destroy();
    }
    if (playout.valid()) {
        playout.destroy();
    }
    if (vshader.valid()) {
        vshader.destroy();
    }
    if (fshader.valid()) {
        fshader.destroy();
    }
    blayouts.clear();
    camera_bind_group = {};
    initialized       = false;
}

void ForwardPass::update_camera_buffer(const RenderView& view)
{
    if (!camera_buffer.valid()) {
        return;
    }

    auto mapped = camera_buffer.get_mapped_range<CameraUniforms>();
    if (mapped.count > 0) {
        mapped.at(0).proj     = view.projection;
        mapped.at(0).view     = view.view;
        mapped.at(0).inv_proj = view.inv_projection;
        mapped.at(0).inv_view = view.inv_view;
        mapped.at(0).pos      = view.camera_position;
        mapped.at(0).padding  = 0.0f;
    }
}

ForwardPassData ForwardPass::add(
    FrameGraphBuilder&       builder,
    const ForwardPassParams& params)
{
    ForwardPassData out{};
    out.color_target = params.color_target;
    out.depth_target = params.depth_target;

    if (!params.color_target.valid() || params.extent.width == 0 || params.extent.height == 0) {
        return out;
    }

    return builder.add_pass<ForwardPassData>(
        "ForwardPass",
        [&](ForwardPassData& data, FrameGraphPassBuilder& pass_builder) {
            auto depth_h = params.depth_target;
            if (!depth_h.valid()) {
                auto depth_desc   = GPUTextureDescriptor{};
                depth_desc.label  = "forward_transient_depth";
                depth_desc.size   = {params.extent.width, params.extent.height, 1};
                depth_desc.format = params.depth_format;
                depth_desc.usage  = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                depth_h           = pass_builder.create<FrameGraphTexture>(depth_desc);
            }

            data.color_target       = pass_builder.render(params.color_target);
            data.depth_target       = pass_builder.render(depth_h);
            data.draw_calls         = static_cast<uint>(params.scene.meshes.size());
            data.triangles_rendered = 0;
            for (const auto& mesh : params.scene.meshes) {
                data.triangles_rendered += mesh.index_count / 3;
            }
        },
        [this, params](const ForwardPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_target);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_target);
            if (!color_tex || !depth_tex) {
                return;
            }

            update_camera_buffer(params.view);

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = color_tex->view;
            color_attachment.clear_value = GPUColor{params.scene.clear_color.r, params.scene.clear_color.g, params.scene.clear_color.b, params.scene.clear_color.a};
            color_attachment.load_op     = params.color_load_op;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = params.depth_prepass_active ? GPULoadOp::LOAD : GPULoadOp::CLEAR;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto pass_desc                     = GPURenderPassDescriptor{};
            pass_desc.color_attachments        = color_attachment;
            pass_desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(pass_desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(params.extent.width), static_cast<float>(params.extent.height));
            command.set_scissor_rect(0, 0, params.extent.width, params.extent.height);
            command.set_pipeline(pipeline);
            if (camera_bind_group.valid()) {
                command.set_bind_group(0, camera_bind_group);
            }

            for (const auto& mesh : params.scene.meshes) {
                if (!mesh.geometry.valid() || mesh.index_count == 0) {
                    continue;
                }

                InstanceData inst{};
                inst.model         = mesh.transform;
                inst.color         = mesh.material_params.base_color;
                inst.shading_model = static_cast<uint>(mesh.shading_model);
                inst.extra_params  = Vector3(mesh.material_params.roughness, mesh.material_params.metallic, 0.0f);

                command.set_immediates(0, inst);

                GPUBuffer vbuf{};
                vbuf.handle = mesh.geometry.vertex_slice.buffer;

                GPUBuffer ibuf{};
                ibuf.handle = mesh.geometry.index_slice.buffer;

                command.set_vertex_buffer(0, vbuf, mesh.geometry.vertex_slice.offset);
                command.set_index_buffer(ibuf, mesh.geometry.index_format, mesh.geometry.index_slice.offset);
                command.draw_indexed(mesh.index_count, 1, mesh.first_index, 0, 0);
            }

            command.end_render_pass();
        });
}
