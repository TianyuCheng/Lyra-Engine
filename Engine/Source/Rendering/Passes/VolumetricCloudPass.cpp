#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Rendering/Passes/VolumetricCloudPass.h>
#include <spdlog/spdlog.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    struct alignas(16) VolumetricCloudUniform
    {
        Matrix4x4 proj;
        Matrix4x4 view;
        Matrix4x4 inv_proj;
        Matrix4x4 inv_view;
        Vector4   camera_pos_and_time;
        Vector4   sun_dir_and_intensity;
        Vector4   sun_color_and_powder;
        Vector4   cloud_altitudes;
        Vector4   cloud_density_and_shape;
        Vector4   cloud_detail_and_wind;
        Vector4   cloud_wind_dir_and_shear;
        Vector4   cloud_lighting_params;
        Vector4   ambient_sky_color;
        Vector4   ambient_ground_color_and_intensity;
        uint32_t  max_steps;
        uint32_t  enable_tonemap;
        uint32_t  padding[2] = {0, 0};
    };

    static_assert(sizeof(VolumetricCloudUniform) == 432, "VolumetricCloudUniform must be 432 bytes");
} // namespace

bool VolumetricCloudPass::init(
    GPUDevice        device,
    Compiler         compiler,
    GPUTextureFormat color_format)
{
    if (initialized) {
        if (cached_color_format == color_format) {
            return true;
        }
        destroy();
    }

    this->device              = device;
    this->cached_color_format = color_format;

    pipeline_manager.init(device, compiler);

    // 1. compile and reflect volumetric cloud pass shaders
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module         = "sys://Shaders/Sky/VolumetricCloudPass.slang";
    shader_desc.vertex_entry   = "vsmain";
    shader_desc.fragment_entry = "fsmain";

    auto layout_info = pipeline_manager.reflect_render_layout(shader_desc);
    if (!layout_info.valid()) {
        spdlog::error("[VolumetricCloudPass] failed to reflect volumetric cloud pass shaders");
        return false;
    }
    blayouts.assign(layout_info.bind_group_layouts.begin(), layout_info.bind_group_layouts.end());

    // 2. linear clamp sampler
    auto sdesc           = GPUSamplerDescriptor{};
    sdesc.label          = "volumetric_cloud_linear_sampler";
    sdesc.min_filter     = GPUFilterMode::LINEAR;
    sdesc.mag_filter     = GPUFilterMode::LINEAR;
    sdesc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
    sdesc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
    sdesc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
    sdesc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
    linear_sampler       = device.create_sampler(sdesc);

    // 3. default 1x1 depth texture fallback
    default_depth_tex  = execute([&]() {
        auto dtex_desc            = GPUTextureDescriptor{};
        dtex_desc.label           = "volumetric_cloud_default_depth";
        dtex_desc.size            = {1, 1, 1};
        dtex_desc.dimension       = GPUTextureDimension::x2D;
        dtex_desc.format          = GPUTextureFormat::R32FLOAT;
        dtex_desc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        dtex_desc.mip_level_count = 1;
        return device.create_texture(dtex_desc);
    });
    default_depth_view = default_depth_tex.create_view();

    execute([&]() {
        auto cmd_desc  = GPUCommandBufferDescriptor{};
        cmd_desc.label = "volumetric_cloud_init_cmd";
        cmd_desc.queue = GPUQueueType::DEFAULT;
        auto cmdbuf    = device.create_command_buffer(cmd_desc);
        cmdbuf.resource_barrier(state_transition(
            default_depth_tex,
            undefined_texture_state(),
            shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        cmdbuf.submit();
        device.wait();
    });

    // 4. render pipeline with hardware alpha blending
    Vector<GPUColorTargetState> rstates;
    auto                        color_state = GPUColorTargetState{};
    color_state.format                      = color_format;
    color_state.write_mask                  = GPUColorWrite::ALL;
    color_state.blend_enable                = true;
    color_state.blend.color.operation       = GPUBlendOperation::ADD;
    color_state.blend.color.src_factor      = GPUBlendFactor::SRC_ALPHA;
    color_state.blend.color.dst_factor      = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
    color_state.blend.alpha.operation       = GPUBlendOperation::ADD;
    color_state.blend.alpha.src_factor      = GPUBlendFactor::ONE;
    color_state.blend.alpha.dst_factor      = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
    rstates.push_back(color_state);

    try {
        pipeline = pipeline_manager.render_pipeline_builder()
                       .with_shader(shader_desc)
                       .with_cull_mode(GPUCullMode::NONE)
                       .with_topology(GPUPrimitiveTopology::TRIANGLE_LIST)
                       .with_front_face(GPUFrontFace::CCW)
                       .with_target(color_state)
                       .without_depth_stencil()
                       .with_label("volumetric_cloud_pipeline")
                       .build();
    } catch (const std::exception& e) {
        spdlog::error("[VolumetricCloudPass] failed to create volumetric cloud render pipeline: {}", e.what());
        return false;
    }

    initialized = pipeline.valid();
    return initialized;
}

void VolumetricCloudPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (default_depth_view.valid()) default_depth_view.destroy();
    if (default_depth_tex.valid()) default_depth_tex.destroy();
    if (linear_sampler.valid()) linear_sampler.destroy();
    pipeline_manager.destroy();
    pipeline = {};

    blayouts.clear();
    initialized = false;
}

auto VolumetricCloudPass::add(FrameGraphBuilder& builder, const VolumetricCloudPassParams& params) -> VolumetricCloudPassData
{
    return builder.add_pass<VolumetricCloudPassData>(
        "VolumetricCloudPass",
        [&](VolumetricCloudPassData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_output = pass_builder.render(params.color_target);
            if (params.depth_target.valid()) {
                (void)pass_builder.sample(params.depth_target);
            }
        },
        [this, params, extent = params.view.render_extent](
            const VolumetricCloudPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid() || blayouts.empty()) {
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_output);
            if (!color_tex) {
                return;
            }

            // resolve directional sunlight from scene
            Vector3 sun_dir       = Vector3(0.0f, 1.0f, 0.0f);
            Vector3 sun_color     = params.sun_color;
            float   sun_intensity = params.sun_intensity;

            for (const auto& light : params.scene.directional_lights) {
                if (light.type == RenderLightType::DIRECTIONAL) {
                    sun_dir       = -glm::normalize(light.direction);
                    sun_color     = light.color;
                    sun_intensity = light.intensity > 1000.0f ? light.intensity / 1000.0f : light.intensity;
                    break;
                }
            }

            // resolve cloud uniform buffer
            VolumetricCloudUniform u{};
            u.proj                               = params.view.projection;
            u.view                               = params.view.view;
            u.inv_proj                           = params.view.inv_projection;
            u.inv_view                           = params.view.inv_view;
            u.camera_pos_and_time                = Vector4(params.view.camera_position, params.time);
            u.sun_dir_and_intensity              = Vector4(sun_dir, sun_intensity);
            u.sun_color_and_powder               = Vector4(sun_color, params.powder_scale);
            u.cloud_altitudes                    = Vector4(params.bottom_altitude, params.top_altitude, params.max_march_distance, params.planet_radius);
            u.cloud_density_and_shape            = Vector4(params.coverage, params.density_scale, params.cloud_type, params.shape_scale);
            u.cloud_detail_and_wind              = Vector4(params.detail_scale, params.detail_strength, params.curl_strength, params.wind_speed);
            u.cloud_wind_dir_and_shear           = Vector4(params.wind_direction, params.height_shear);
            u.cloud_lighting_params              = Vector4(params.phase_g_forward, params.phase_g_backward, params.phase_blend, params.multi_scatter_boost);
            u.ambient_sky_color                  = Vector4(params.ambient_sky_color, 0.0f);
            u.ambient_ground_color_and_intensity = Vector4(params.ambient_ground_color, params.ambient_intensity);
            u.max_steps                          = params.max_steps;
            u.enable_tonemap                     = params.enable_tonemap ? 1u : 0u;

            auto u_slice = ctx->create_uniform(u);

            // resolve depth texture
            GPUTextureView depth_view = default_depth_view;
            if (params.depth_target.valid()) {
                auto depth_tex = resources.get<FrameGraphTexture>(params.depth_target);
                if (depth_tex && depth_tex->view.valid()) {
                    depth_view = depth_tex->view;
                }
            }

            Array<GPUBindGroupEntry, 3> entries{};
            entries.at(0).type    = GPUResourceType::BUFFER;
            entries.at(0).binding = 0;
            entries.at(0).buffer  = u_slice;

            entries.at(1).type    = GPUResourceType::TEXTURE;
            entries.at(1).binding = 1;
            entries.at(1).texture = depth_view;

            entries.at(2).type    = GPUResourceType::SAMPLER;
            entries.at(2).binding = 2;
            entries.at(2).sampler = linear_sampler;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.layout  = blayouts.at(0);
            bg_desc.entries = entries;
            auto bg         = ctx->allocate(bg_desc);

            auto color_att        = GPURenderPassColorAttachment{};
            color_att.view        = color_tex->view;
            color_att.load_op     = params.color_load_op;
            color_att.store_op    = GPUStoreOp::STORE;
            color_att.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};

            auto pass_desc              = GPURenderPassDescriptor{};
            pass_desc.color_attachments = color_att;

            auto command = ctx->cmdlist;
            command.begin_render_pass(pass_desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            if (bg.valid()) {
                command.set_bind_group(0, bg);
            }
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });
}
