#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/ForwardPass.h>
#include <Lyra/Rendering/ShadingModelRegistry.h>
#include <spdlog/spdlog.h>

using namespace lyra;
using namespace lyra::passes;

bool ForwardPass::init(
    GPUDevice        dev,
    Compiler         in_compiler,
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
    compiler            = in_compiler;
    cached_color_format = color_format;
    cached_depth_format = depth_format;

    // reflect pass shell to get canonical bind group layouts and immediate size
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "Forward";
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

    // bind group layouts
    blayouts.clear();
    auto refl_layouts = reflection->get_bind_group_layouts();
    for (usize g = 0; g < refl_layouts.size(); ++g) {
        auto blayout = device.create_bind_group_layout(refl_layouts[g]);
        blayouts.push_back(blayout);
    }

    playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = blayouts;
        desc.immediate_size     = reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    // initialize MeshPass with vertex bind group layout (Bind Group 1)
    if (blayouts.size() > 1) {
        mesh_pass.init(device, blayouts[1]);
    }

    // camera uniform buffers and bind groups
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

    shadow_sampler = execute([&]() {
        auto desc           = GPUSamplerDescriptor{};
        desc.label          = "forward_shadow_sampler";
        desc.min_filter     = GPUFilterMode::LINEAR;
        desc.mag_filter     = GPUFilterMode::LINEAR;
        desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        return device.create_sampler(desc);
    });

    default_shadow_map  = execute([&]() {
        auto desc   = GPUTextureDescriptor{};
        desc.label  = "forward_default_shadow_map";
        desc.size   = {1, 1, 1};
        desc.format = GPUTextureFormat::DEPTH32FLOAT;
        desc.usage  = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::RENDER_ATTACHMENT;
        return device.create_texture(desc);
    });
    default_shadow_view = default_shadow_map.create_view();

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 512;
        return device.create_bind_group_heap(desc);
    });

    initialized = true;
    return true;
}

void ForwardPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    mesh_pass.destroy();

    for (auto& [_, pipe] : variant_pipelines) {
        if (pipe.valid()) {
            pipe.destroy();
        }
    }
    variant_pipelines.clear();

    for (auto& sm : variant_shader_modules) {
        if (sm.valid()) {
            sm.destroy();
        }
    }
    variant_shader_modules.clear();

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

    if (shadow_sampler.valid()) {
        shadow_sampler.destroy();
    }
    if (default_shadow_view.valid()) {
        default_shadow_view.destroy();
    }
    if (default_shadow_map.valid()) {
        default_shadow_map.destroy();
    }

    if (playout.valid()) {
        playout.destroy();
    }
    blayouts.clear();
    initialized = false;
}

void ForwardPass::update_camera_buffer(uint slot, const RenderView& view, const RenderScene& scene, const Matrix4x4& light_vp)
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

        float   exposure_scale = (view.exposure_scale > 0.0f) ? view.exposure_scale : (1.0f / (1.2f * 8192.0f));
        Vector3 sun_dir        = glm::normalize(Vector3(0.6f, 1.0f, 0.5f));
        Vector3 sun_color      = Vector3(1.0f);
        float   sun_intensity  = 110000.0f * exposure_scale;

        if (!scene.directional_lights.empty()) {
            const auto& light = scene.directional_lights[0];
            if (glm::length2(light.direction) > 1e-4f) {
                sun_dir = -glm::normalize(light.direction);
            }
            sun_color     = light.color;
            sun_intensity = std::max(light.intensity * exposure_scale, 0.01f);
        }

        mapped.at(0).sun_intensity   = sun_intensity;
        mapped.at(0).sun_dir         = sun_dir;
        mapped.at(0).padding         = 0.0f;
        mapped.at(0).sun_color       = sun_color;
        mapped.at(0).padding2        = 0.0f;
        mapped.at(0).light_view_proj = light_vp;
    }
}

GPURenderPipeline ForwardPass::get_or_create_pipeline(GUID shading_model_id)
{
    auto it = variant_pipelines.find(shading_model_id);
    if (it != variant_pipelines.end() && it->second.valid()) {
        return it->second;
    }

    auto model_info = ShadingModelRegistry::get_model(shading_model_id);
    if (!model_info) {
        model_info = ShadingModelRegistry::get_model(BuiltinShadingModels::PBR);
    }

    Path   shader_path = model_info ? model_info->shader_module_path : Path("sys://Shaders/Forward/Forward.slang");
    String entry_point = model_info ? model_info->entry_point_name : String("fs_pbr");

    auto shader_stem     = shader_path.stem().string();
    auto shader_path_str = shader_path.string();
    auto desc            = CompileDescriptor{};
    desc.module          = shader_stem.c_str();
    desc.path            = shader_path_str.c_str();
    desc.source          = nullptr;

    auto compiled_module = execute([&]() {
        return compiler.compile(desc);
    });

    if (!compiled_module) {
        return {};
    }

    auto vshader = execute([&]() {
        auto code   = compiled_module->get_shader_blob("vsmain");
        auto sdesc  = GPUShaderModuleDescriptor{};
        sdesc.label = "forward_vertex_shader";
        sdesc.data  = code->data;
        sdesc.size  = code->size;
        return device.create_shader_module(sdesc);
    });

    auto fshader = execute([&]() {
        auto code   = compiled_module->get_shader_blob(entry_point.c_str());
        auto sdesc  = GPUShaderModuleDescriptor{};
        sdesc.label = "forward_fragment_shader";
        sdesc.data  = code->data;
        sdesc.size  = code->size;
        return device.create_shader_module(sdesc);
    });

    if (!vshader.valid() || !fshader.valid()) {
        return {};
    }

    variant_shader_modules.push_back(vshader);
    variant_shader_modules.push_back(fshader);

    auto pipeline = execute([&]() {
        Vector<GPUColorTargetState> targets;
        auto                        color_target = GPUColorTargetState{};
        color_target.format                      = cached_color_format;
        color_target.blend_enable                = false;
        targets.push_back(color_target);

        auto pdesc                              = GPURenderPipelineDescriptor{};
        pdesc.layout                            = playout;
        pdesc.primitive.cull_mode               = GPUCullMode::BACK;
        pdesc.primitive.topology                = GPUPrimitiveTopology::TRIANGLE_LIST;
        pdesc.primitive.front_face              = GPUFrontFace::CCW;
        pdesc.primitive.strip_index_format      = GPUIndexFormat::UINT32;
        pdesc.depth_stencil.depth_compare       = GPUCompareFunction::LESS_EQUAL;
        pdesc.depth_stencil.depth_write_enabled = true;
        pdesc.depth_stencil.format              = cached_depth_format;
        pdesc.multisample.count                 = 1;
        pdesc.vertex.module                     = vshader;
        pdesc.vertex.buffers                    = {};
        pdesc.vertex.entry_point                = "vsmain";
        pdesc.fragment.module                   = fshader;
        pdesc.fragment.entry_point              = entry_point.c_str();
        pdesc.fragment.targets                  = targets;
        return device.create_render_pipeline(pdesc);
    });

    if (pipeline.valid()) {
        variant_pipelines[shading_model_id] = pipeline;
    }

    return pipeline;
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

    frame_slot        = (frame_slot + 1) % FRAMES_IN_FLIGHT;
    uint current_slot = frame_slot;

    update_camera_buffer(current_slot, params.view, params.scene, params.light_view_proj);

    // prepare sorted and batched mesh commands via MeshPass
    mesh_pass.prepare(
        device,
        params.scene,
        params.view,
        [this](const MeshPrimitive& mesh) -> GPURenderPipeline {
            GUID model_id = mesh.shading_model_id != 0
                                ? mesh.shading_model_id
                                : BuiltinShadingModels::from_enum(mesh.shading_model);
            return get_or_create_pipeline(model_id);
        });

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

            data.color_target = pass_builder.render(params.color_target);
            data.depth_target = pass_builder.render(depth_h);
            if (params.shadow_map.valid()) {
                data.shadow_map = pass_builder.sample(params.shadow_map);
            }
            data.draw_calls         = mesh_pass.get_draw_call_count();
            data.triangles_rendered = mesh_pass.get_triangle_count();
        },
        [this, current_slot, params](const ForwardPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid()) {
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_target);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_target);
            if (!color_tex || !depth_tex) {
                return;
            }

            GPUTextureViewHandle s_view = default_shadow_view.handle;
            if (data.shadow_map.valid()) {
                if (auto shadow_tex = resources.get<FrameGraphTexture>(data.shadow_map)) {
                    s_view = shadow_tex->view;
                }
            }

            Array<GPUBindGroupEntry, 3> entries{};
            entries[0].type          = GPUResourceType::BUFFER;
            entries[0].binding       = 0;
            entries[0].buffer.buffer = camera_buffers[current_slot].handle;
            entries[0].buffer.offset = 0;
            entries[0].buffer.size   = sizeof(CameraUniforms);

            entries[1].type    = GPUResourceType::TEXTURE;
            entries[1].binding = 1;
            entries[1].texture = s_view;

            entries[2].type    = GPUResourceType::SAMPLER;
            entries[2].binding = 2;
            entries[2].sampler = shadow_sampler.handle;

            auto bg_desc = GPUBindGroupDescriptor{};
            bg_desc.heap = heap;
            if (!blayouts.empty()) {
                bg_desc.layout = blayouts[0];
            }
            bg_desc.entries = entries;
            auto pass_bg    = device.create_bind_group(bg_desc);

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

            // delegate all mesh draws and state transitions to MeshPass with pass camera bind group (Set 0)
            mesh_pass.draw(command, pass_bg);

            command.end_render_pass();
        });
}
