#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/ForwardPass.h>
#include <spdlog/spdlog.h>

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
    auto refl_layouts = reflection->get_bind_group_layouts();
    spdlog::info("[ForwardPass] reflected {} bind group layouts", refl_layouts.size());
    for (usize g = 0; g < refl_layouts.size(); ++g) {
        for (const auto& entry : refl_layouts[g].entries) {
            spdlog::info("[ForwardPass] group {} entry binding={}, type={}", g, entry.binding.index, static_cast<uint>(entry.type));
        }
        auto blayout = device.create_bind_group_layout(refl_layouts[g]);
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
        Vector<GPUColorTargetState> targets;

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
        desc.vertex.buffers                    = {}; // vertex pulling: no input assembler vertex layouts
        desc.vertex.entry_point                = "vsmain";
        desc.fragment.module                   = fshader;
        desc.fragment.entry_point              = "fsmain";
        desc.fragment.targets                  = targets;

        return device.create_render_pipeline(desc);
    });

    if (!pipeline.valid()) {
        return false;
    }

    // 4. camera uniform buffers and bind groups (multi-buffered for frames in flight)
    for (uint i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        camera_buffers[i] = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "forward_camera_uniform_buffer";
            desc.size               = sizeof(CameraUniforms);
            desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });
    }

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 512;
        return device.create_bind_group_heap(desc);
    });

    if (!blayouts.empty()) {
        for (uint i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            camera_bind_groups[i] = execute([&]() {
                Array<GPUBindGroupEntry, 1> entries{};
                entries[0].type          = GPUResourceType::BUFFER;
                entries[0].binding       = 0;
                entries[0].buffer.buffer = camera_buffers[i].handle;
                entries[0].buffer.offset = 0;
                entries[0].buffer.size   = sizeof(CameraUniforms);

                auto desc    = GPUBindGroupDescriptor{};
                desc.heap    = heap;
                desc.layout  = blayouts[0];
                desc.entries = entries;
                return device.create_bind_group(desc);
            });
        }
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
    for (auto& buf : camera_buffers) {
        if (buf.valid()) {
            buf.destroy();
        }
    }
    camera_buffers.fill({});
    camera_bind_groups.fill({});

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
    geometry_bind_groups.clear();
    initialized = false;
}

void ForwardPass::update_camera_buffer(uint slot, const RenderView& view, const RenderScene& scene)
{
    if (slot >= FRAMES_IN_FLIGHT || !camera_buffers[slot].valid()) {
        return;
    }

    auto mapped = camera_buffers[slot].get_mapped_range<CameraUniforms>();
    if (mapped.count > 0) {
        mapped.at(0).proj     = view.projection;
        mapped.at(0).view     = view.view;
        mapped.at(0).inv_proj = view.inv_projection;
        mapped.at(0).inv_view = view.inv_view;
        mapped.at(0).pos      = view.camera_position;

        Vector3 sun_dir       = glm::normalize(Vector3(0.5f, 1.0f, 0.3f));
        Vector3 sun_color     = Vector3(1.0f);
        float   sun_intensity = 2.5f;

        if (!scene.directional_lights.empty()) {
            const auto& light = scene.directional_lights[0];
            if (glm::length2(light.direction) > 1e-4f) {
                // Vector pointing to sun is opposite of propagation direction
                sun_dir = -glm::normalize(light.direction);
            }
            sun_color     = light.color;
            sun_intensity = std::max(light.intensity * 0.0025f, 0.1f);
        }

        mapped.at(0).sun_intensity = sun_intensity;
        mapped.at(0).sun_dir       = sun_dir;
        mapped.at(0).padding       = 0.0f;
        mapped.at(0).sun_color     = sun_color;
        mapped.at(0).padding2      = 0.0f;
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

    frame_slot = (frame_slot + 1) % FRAMES_IN_FLIGHT;
    uint current_slot = frame_slot;

    update_camera_buffer(current_slot, params.view, params.scene);

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
        [this, current_slot, params](const ForwardPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_target);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_target);
            if (!color_tex || !depth_tex) {
                return;
            }

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = color_tex->view;
            color_attachment.clear_value = GPUColor{params.scene.clear_color.r, params.scene.clear_color.g, params.scene.clear_color.b, params.scene.clear_color.a};
            color_attachment.load_op     = params.color_load_op;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = params.depth_load_op;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto pass_desc                     = GPURenderPassDescriptor{};
            pass_desc.color_attachments        = color_attachment;
            pass_desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(pass_desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(params.extent.width), static_cast<float>(params.extent.height));
            command.set_scissor_rect(0, 0, params.extent.width, params.extent.height);
            command.set_pipeline(pipeline);
            if (camera_bind_groups[current_slot].valid()) {
                command.set_bind_group(0, camera_bind_groups[current_slot]);
            }

            uint64_t last_bound_vbuf = ~0ull;

            for (const auto& mesh : params.scene.meshes) {
                if (!mesh.geometry.valid() || mesh.index_count == 0) {
                    continue;
                }

                GPUBufferHandle vb_handle = mesh.geometry.vertex_slice.buffer;
                if (!vb_handle.valid()) {
                    continue;
                }

                // bind group 1: vertex buffer storage resource
                if (blayouts.size() > 1) {
                    auto it = geometry_bind_groups.find(vb_handle.value);
                    if (it == geometry_bind_groups.end()) {
                        Array<GPUBindGroupEntry, 1> entries{};
                        entries[0].type          = GPUResourceType::BUFFER;
                        entries[0].binding       = 0;
                        entries[0].buffer.buffer = vb_handle;
                        entries[0].buffer.offset = 0;
                        entries[0].buffer.size   = 0;

                        auto bg_desc    = GPUBindGroupDescriptor{};
                        bg_desc.heap    = heap;
                        bg_desc.layout  = blayouts[1];
                        bg_desc.entries = entries;
                        it              = geometry_bind_groups.emplace(vb_handle.value, device.create_bind_group(bg_desc)).first;
                    }

                    if (it != geometry_bind_groups.end() && last_bound_vbuf != vb_handle.value) {
                        command.set_bind_group(1, it->second);
                        last_bound_vbuf = vb_handle.value;
                    }
                }

                const auto& offsets     = mesh.geometry.stream_offsets;
                GPUSize64   base_offset = mesh.geometry.vertex_slice.offset;

                InstanceData inst{};
                inst.model       = mesh.transform;
                inst.color       = mesh.material_params.base_color;
                inst.params      = Vector4(
                    mesh.material_params.roughness,
                    mesh.material_params.metallic,
                    0.0f,
                    glm::uintBitsToFloat(static_cast<uint>(mesh.shading_model)));
                inst.pos_offset  = static_cast<uint>(base_offset + (offsets.has_position() ? offsets.position : 0));
                inst.norm_offset = static_cast<uint>(base_offset + (offsets.has_normal() ? offsets.normal : inst.pos_offset));
                inst.uv_offset   = static_cast<uint>(base_offset + (offsets.has_texcoord0() ? offsets.texcoord0 : inst.pos_offset));

                command.set_immediates(0, inst);

                GPUBuffer ibuf{};
                ibuf.handle = mesh.geometry.index_slice.buffer;

                command.set_index_buffer(ibuf, mesh.geometry.index_format, mesh.geometry.index_slice.offset);
                command.draw_indexed(mesh.index_count, 1, mesh.first_index, 0, 0);
            }

            command.end_render_pass();
        });
}
