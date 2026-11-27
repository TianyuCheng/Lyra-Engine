#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/ForwardPass.h>
#include <Lyra/Rendering/ShadingModelRegistry.h>
#include <Lyra/Rendering/SceneLighting.h>
#include <spdlog/spdlog.h>

using namespace lyra;
using namespace lyra::passes;

bool ForwardPass::init(
    GPUDevice                   dev,
    Compiler                    in_compiler,
    GPUTextureFormat            color_format,
    GPUTextureFormat            depth_format,
    const Path&                 shaders_root,
    Ref<GeometryBindGroupCache> cache)
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

    pipeline_manager.init(device, compiler);

    // reflect pass shell to get canonical bind group layouts and immediate size
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module         = "sys://Shaders/Forward/Forward.slang";
    shader_desc.vertex_entry   = "vsmain";
    shader_desc.fragment_entry = "fsmain";

    auto layout = pipeline_manager.reflect_render_layout(shader_desc);
    if (!layout) {
        return false;
    }
    blayouts.assign(layout->bind_group_layouts.begin(), layout->bind_group_layouts.end());
    playout = layout->layout;

    // initialize MeshPass with vertex bind group layout (Bind Group 1)
    if (blayouts.size() > 1) {
        mesh_pass.init(device, blayouts[1], cache);
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

    default_cluster_grid_buffer = execute([&]() {
        auto desc  = GPUBufferDescriptor{};
        desc.label = "forward_default_cluster_grid_buffer";
        desc.size  = sizeof(uint) * 2;
        desc.usage = GPUBufferUsage::STORAGE;
        return device.create_buffer(desc);
    });

    default_light_index_buffer = execute([&]() {
        auto desc  = GPUBufferDescriptor{};
        desc.label = "forward_default_light_index_buffer";
        desc.size  = sizeof(uint);
        desc.usage = GPUBufferUsage::STORAGE;
        return device.create_buffer(desc);
    });

    environment_sampler = execute([&]() {
        auto desc           = GPUSamplerDescriptor{};
        desc.label          = "forward_environment_sampler";
        desc.min_filter     = GPUFilterMode::LINEAR;
        desc.mag_filter     = GPUFilterMode::LINEAR;
        desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
        desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
        return device.create_sampler(desc);
    });

    default_env_map = execute([&]() {
        auto desc            = GPUTextureDescriptor{};
        desc.label           = "forward_default_env_map";
        desc.size            = {1, 1, 1};
        desc.dimension       = GPUTextureDimension::x2D;
        desc.array_layers    = 6;
        desc.format          = GPUTextureFormat::RGBA16FLOAT;
        desc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        desc.mip_level_count = 1;
        return device.create_texture(desc);
    });
    GPUTextureViewDescriptor env_vdesc{};
    env_vdesc.aspect            = GPUTextureAspect::COLOR;
    env_vdesc.dimension         = GPUTextureViewDimension::CUBE;
    env_vdesc.format            = GPUTextureFormat::RGBA16FLOAT;
    env_vdesc.base_array_layer  = 0;
    env_vdesc.array_layer_count = 6;
    env_vdesc.base_mip_level    = 0;
    env_vdesc.mip_level_count   = 1;
    default_env_view            = default_env_map.create_view(env_vdesc);

    default_brdf_lut      = execute([&]() {
        auto desc            = GPUTextureDescriptor{};
        desc.label           = "forward_default_brdf_lut";
        desc.size            = {1, 1, 1};
        desc.dimension       = GPUTextureDimension::x2D;
        desc.format          = GPUTextureFormat::RGBA16FLOAT;
        desc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        desc.mip_level_count = 1;
        return device.create_texture(desc);
    });
    default_brdf_lut_view = default_brdf_lut.create_view();

    execute([&]() {
        auto cmd_desc  = GPUCommandBufferDescriptor{};
        cmd_desc.label = "forward_default_env_transition_cmd";
        cmd_desc.queue = GPUQueueType::DEFAULT;
        auto cmdbuf    = device.create_command_buffer(cmd_desc);
        cmdbuf.resource_barrier(state_transition(default_env_map, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING), 0, 6));
        cmdbuf.resource_barrier(state_transition(default_brdf_lut, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        cmdbuf.submit();
        device.wait();
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

    if (shadow_sampler.valid()) {
        shadow_sampler.destroy();
    }
    if (environment_sampler.valid()) {
        environment_sampler.destroy();
    }
    if (default_env_view.valid()) {
        default_env_view.destroy();
    }
    if (default_env_map.valid()) {
        default_env_map.destroy();
    }
    if (default_brdf_lut_view.valid()) {
        default_brdf_lut_view.destroy();
    }
    if (default_brdf_lut.valid()) {
        default_brdf_lut.destroy();
    }
    if (default_light_index_buffer.valid()) {
        default_light_index_buffer.destroy();
    }
    if (default_cluster_grid_buffer.valid()) {
        default_cluster_grid_buffer.destroy();
    }
    if (default_shadow_view.valid()) {
        default_shadow_view.destroy();
    }
    if (default_shadow_map.valid()) {
        default_shadow_map.destroy();
    }

    pipeline_manager.destroy();
    playout = {};
    variant_pipelines.clear();
    variant_shader_modules.clear();
    blayouts.clear();
    initialized = false;
}

ForwardPass::ForwardPassUniforms ForwardPass::make_uniforms(
    const RenderView&  view,
    const RenderScene& scene,
    const Matrix4x4&   light_vp,
    const GPUExtent2D& extent,
    uint               cluster_grid_x,
    uint               cluster_grid_y,
    uint               cluster_grid_z,
    uint               num_lights) const
{
    ForwardPassUniforms u{};
    u.proj                 = view.projection;
    u.view                 = view.view;
    u.inv_proj             = view.inv_projection;
    u.inv_view             = view.inv_view;
    float   exposure_scale = (view.exposure_scale > 0.0f) ? view.exposure_scale : (1.0f / (1.2f * 8192.0f));
    Vector3 sun_dir        = glm::normalize(Vector3(0.6f, 1.0f, 0.5f));
    Vector3 sun_color      = Vector3(1.0f);
    float   sun_intensity  = 0.0f;

    if (!scene.directional_lights.empty()) {
        const auto& light = scene.directional_lights[0];
        if (glm::length2(light.direction) > 1e-4f) {
            sun_dir = -glm::normalize(light.direction);
        }
        sun_color     = light.color;
        sun_intensity = std::max(light.intensity * exposure_scale, 0.0f);
    }

    u.pos_and_intensity = Vector4(view.camera_position, sun_intensity);
    u.sun_dir_and_pad   = Vector4(sun_dir, 0.0f);
    u.sun_color_and_pad = Vector4(sun_color, 0.0f);
    u.light_view_proj   = light_vp;

    float near_p = view.near_plane > 0.0f ? view.near_plane : 0.1f;
    float far_p  = view.far_plane > near_p ? view.far_plane : 100.0f;

    u.cluster_params = Vector4(
        static_cast<float>(extent.width),
        static_cast<float>(extent.height),
        near_p,
        far_p);

    u.cluster_dims = Vector4(
        static_cast<float>(cluster_grid_x),
        static_cast<float>(cluster_grid_y),
        static_cast<float>(cluster_grid_z),
        static_cast<float>(num_lights));

    for (size_t i = 0; i < 9; ++i) {
        u.env_sh[i] = scene.env_sh[i];
    }
    return u;
}

GPURenderPipeline ForwardPass::get_or_create_pipeline(ShadingModelID shading_model_id)
{
    auto it = variant_pipelines.find(shading_model_id);
    if (it != variant_pipelines.end() && it->second.valid()) {
        return it->second;
    }

    auto model_info = ShadingModelRegistry::get_model(shading_model_id);
    if (!model_info) {
        model_info = ShadingModelRegistry::get_model(BuiltinShadingModels::STANDARD);
    }

    Path   shader_path = model_info ? model_info->shader_module_path : Path("sys://Shaders/Forward/Forward.slang");
    String entry_point = model_info ? model_info->entry_point_name : String("fs_standard");

    auto shader_stem     = shader_path.stem().string();
    auto shader_path_str = shader_path.string();
    auto desc            = CompileDescriptor{};
    desc.module          = shader_stem.c_str();
    desc.path            = shader_path_str.c_str();
    desc.source          = nullptr;

    auto compiled_module = compiler.compile(desc);
    if (!compiled_module) {
        return {};
    }

    auto vshader = execute([&]() -> GPUShaderModule {
        auto code = (*compiled_module)->get_shader_blob("vsmain");
        if (!code) return {};
        auto sdesc  = GPUShaderModuleDescriptor{};
        sdesc.label = "forward_vertex_shader";
        sdesc.data  = code->data;
        sdesc.size  = code->size;
        return device.create_shader_module(sdesc);
    });

    auto fshader = execute([&]() -> GPUShaderModule {
        auto code = (*compiled_module)->get_shader_blob(entry_point.c_str());
        if (!code) return {};
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

        auto color_target         = GPUColorTargetState{};
        color_target.format       = cached_color_format;
        color_target.blend_enable = false;
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

    auto forward_uniforms = make_uniforms(
        params.view,
        params.scene,
        params.light_view_proj,
        params.extent,
        params.cluster_grid_x,
        params.cluster_grid_y,
        params.cluster_grid_z,
        params.num_lights);

    // prepare sorted and batched mesh commands via MeshPass
    mesh_pass.prepare(
        device,
        params.scene,
        params.view,
        [this](const MeshPrimitive& mesh) -> GPURenderPipeline {
            ShadingModelID model_id = mesh.shading_model_id != 0
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
            if (params.cluster_grid_buffer.valid()) {
                data.cluster_grid_buffer = pass_builder.read(params.cluster_grid_buffer);
            }
            if (params.light_index_list.valid()) {
                data.light_index_list = pass_builder.read(params.light_index_list);
            }
            data.draw_calls         = mesh_pass.get_draw_call_count();
            data.triangles_rendered = mesh_pass.get_triangle_count();
        },
        [this, forward_uniforms, params](const ForwardPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
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

            GPUBufferHandle grid_buf_h  = default_cluster_grid_buffer.handle;
            GPUBufferHandle index_buf_h = default_light_index_buffer.handle;

            if (data.cluster_grid_buffer.valid()) {
                if (auto b = resources.get<FrameGraphBuffer>(data.cluster_grid_buffer)) {
                    grid_buf_h = b->buffer;
                }
            }
            if (data.light_index_list.valid()) {
                if (auto b = resources.get<FrameGraphBuffer>(data.light_index_list)) {
                    index_buf_h = b->buffer;
                }
            }

            auto active_lights = params.lights_slice;
            if (!active_lights.valid() && ctx->scene_lighting && ctx->scene_lighting->lights_slice.valid()) {
                active_lights = ctx->scene_lighting->lights_slice;
            }
            if (!active_lights.valid() && ctx->arena) {
                SceneLighting sl{};
                sl.init(params.scene, *ctx->arena, ctx->device);
                active_lights = sl.lights_slice;
            }

            GPUTextureViewHandle env_view = default_env_view.handle;
            if (params.environment_map.valid()) {
                env_view = params.environment_map;
            } else if (params.scene.environment_map.valid()) {
                env_view = params.scene.environment_map;
            }

            GPUTextureViewHandle brdf_view = default_brdf_lut_view.handle;
            if (params.brdf_lut.valid()) {
                brdf_view = params.brdf_lut;
            }

            auto u_slice = ctx->create_uniform(forward_uniforms);

            Array<GPUBindGroupEntry, 9> entries{};
            entries[0].type    = GPUResourceType::BUFFER;
            entries[0].binding = 0;
            entries[0].buffer  = u_slice;

            entries[1].type    = GPUResourceType::TEXTURE;
            entries[1].binding = 1;
            entries[1].texture = s_view;

            entries[2].type    = GPUResourceType::SAMPLER;
            entries[2].binding = 2;
            entries[2].sampler = shadow_sampler.handle;

            entries[3].type          = GPUResourceType::BUFFER;
            entries[3].binding       = 3;
            entries[3].buffer.buffer = grid_buf_h;
            entries[3].buffer.offset = 0;
            entries[3].buffer.size   = 0;

            entries[4].type          = GPUResourceType::BUFFER;
            entries[4].binding       = 4;
            entries[4].buffer.buffer = index_buf_h;
            entries[4].buffer.offset = 0;
            entries[4].buffer.size   = 0;

            entries[5].type    = GPUResourceType::BUFFER;
            entries[5].binding = 5;
            entries[5].buffer  = active_lights;

            entries[6].type    = GPUResourceType::TEXTURE;
            entries[6].binding = 6;
            entries[6].texture = env_view;

            entries[7].type    = GPUResourceType::TEXTURE;
            entries[7].binding = 7;
            entries[7].texture = brdf_view;

            entries[8].type    = GPUResourceType::SAMPLER;
            entries[8].binding = 8;
            entries[8].sampler = environment_sampler.handle;

            auto bg_desc = GPUBindGroupDescriptor{};
            if (!blayouts.empty()) {
                bg_desc.layout = blayouts[0];
            }
            bg_desc.entries = entries;
            auto pass_bg    = ctx->allocate(bg_desc);

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
