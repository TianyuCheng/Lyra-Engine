#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Rendering/Passes/SkyboxPass.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    struct SkyboxUniform
    {
        Matrix4x4 proj;
        Matrix4x4 view;
        Matrix4x4 inv_proj;
        Matrix4x4 inv_view;
        Vector4   camera_pos_and_sun_intensity;
        Vector4   sun_dir_and_angular_radius;
        Vector4   sun_color_and_exposure_scale;
        Vector4   sky_params;
        Vector4   ground_albedo = Vector4(0.12f, 0.12f, 0.14f, 1.0f);
    };
} // namespace

bool SkyboxPass::init(
    GPUDevice        device,
    Compiler         compiler,
    GPUTextureFormat color_format,
    GPUTextureFormat depth_format)
{
    if (initialized) {
        if (cached_color_format == color_format && cached_depth_format == depth_format) {
            return true;
        }
        destroy();
    }

    this->device              = device;
    this->cached_color_format = color_format;
    this->cached_depth_format = depth_format;

    auto compile_shader = [&](CString module_name, CString path,
                              GPUShaderModule& out_vs, GPUShaderModule& out_fs,
                              Vector<GPUBindGroupLayoutHandle>& out_blayouts,
                              GPUPipelineLayout&                out_playout) -> bool {
        auto desc   = CompileDescriptor{};
        desc.module = module_name;
        desc.path   = path;
        desc.source = nullptr;
        auto module = compiler.compile(desc);
        if (!module) return false;

        auto reflection = compiler.reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });
        if (!reflection) return false;

        auto vcode = module->get_shader_blob("vsmain");
        auto fcode = module->get_shader_blob("fsmain");
        if (!vcode || !fcode) return false;

        auto vs_desc  = GPUShaderModuleDescriptor{};
        vs_desc.label = module_name;
        vs_desc.data  = vcode->data;
        vs_desc.size  = vcode->size;
        out_vs        = device.create_shader_module(vs_desc);

        auto fs_desc  = GPUShaderModuleDescriptor{};
        fs_desc.label = module_name;
        fs_desc.data  = fcode->data;
        fs_desc.size  = fcode->size;
        out_fs        = device.create_shader_module(fs_desc);

        out_blayouts.clear();
        for (auto& bdesc : reflection->get_bind_group_layouts()) {
            out_blayouts.push_back(device.create_bind_group_layout(bdesc));
        }

        auto pdesc               = GPUPipelineLayoutDescriptor{};
        pdesc.bind_group_layouts = out_blayouts;
        pdesc.immediate_size     = reflection->get_immediate_size();
        out_playout              = device.create_pipeline_layout(pdesc);
        return true;
    };

    auto create_pipeline = [&](CString label, GPUPipelineLayout layout,
                               GPUShaderModule vs, GPUShaderModule fs,
                               GPUTextureFormat color_fmt,
                               GPUTextureFormat depth_fmt  = GPUTextureFormat::DEPTH32FLOAT,
                               bool             depth_test = false) -> GPURenderPipeline {
        Vector<GPUColorTargetState> rstates;
        auto                        color_state = GPUColorTargetState{};
        color_state.format                      = color_fmt;
        color_state.blend_enable                = false;
        rstates.push_back(color_state);

        auto desc                         = GPURenderPipelineDescriptor{};
        desc.label                        = label;
        desc.layout                       = layout;
        desc.primitive.cull_mode          = GPUCullMode::NONE;
        desc.primitive.topology           = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face         = GPUFrontFace::CCW;
        desc.primitive.strip_index_format = GPUIndexFormat::UINT32;
        if (depth_test) {
            desc.depth_stencil.depth_compare       = GPUCompareFunction::LESS_EQUAL;
            desc.depth_stencil.depth_write_enabled = false;
            desc.depth_stencil.format              = depth_fmt;
        }
        desc.multisample.count    = 1;
        desc.vertex.module        = vs;
        desc.vertex.entry_point   = "vsmain";
        desc.fragment.module      = fs;
        desc.fragment.targets     = rstates;
        desc.fragment.entry_point = "fsmain";
        return device.create_render_pipeline(desc);
    };

    auto create_lut_texture = [&](uint width, uint height, GPUTextureFormat format, CString label) -> GPUTexture {
        auto desc            = GPUTextureDescriptor{};
        desc.label           = label;
        desc.size            = {width, height, 1};
        desc.format          = format;
        desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
        desc.dimension       = GPUTextureDimension::x2D;
        desc.mip_level_count = 1;
        desc.sample_count    = 1;
        return device.create_texture(desc);
    };

    // 1. compile and build 3-LUT pipelines
    if (!compile_shader("atmo_transmittance", "sys://Shaders/Sky/AtmosphereTransmittanceLUT.slang",
            transmittance_vshader, transmittance_fshader,
            transmittance_blayouts, transmittance_playout)) {
        return false;
    }
    transmittance_pipeline = create_pipeline("atmo_transmittance_pipeline", transmittance_playout,
        transmittance_vshader, transmittance_fshader,
        GPUTextureFormat::RGBA16FLOAT);

    if (!compile_shader("atmo_multiscatter", "sys://Shaders/Sky/AtmosphereMultiScatterLUT.slang",
            multiscatter_vshader, multiscatter_fshader,
            multiscatter_blayouts, multiscatter_playout)) {
        return false;
    }
    multiscatter_pipeline = create_pipeline("atmo_multiscatter_pipeline", multiscatter_playout,
        multiscatter_vshader, multiscatter_fshader,
        GPUTextureFormat::RGBA16FLOAT);

    if (!compile_shader("atmo_sky_view", "sys://Shaders/Sky/AtmosphereSkyViewLUT.slang",
            sky_view_vshader, sky_view_fshader,
            sky_view_blayouts, sky_view_playout)) {
        return false;
    }
    sky_view_pipeline = create_pipeline("atmo_sky_view_pipeline", sky_view_playout,
        sky_view_vshader, sky_view_fshader,
        GPUTextureFormat::RGBA16FLOAT);

    // 2. compile and build screen Skybox pipeline
    if (!compile_shader("skybox", "sys://Shaders/Sky/Skybox.slang",
            vshader, fshader, blayouts, playout)) {
        return false;
    }
    pipeline = create_pipeline("skybox_render_pipeline", playout, vshader, fshader,
        cached_color_format, cached_depth_format, true);

    // 3. create static textures and views
    transmittance_lut_tex  = create_lut_texture(256, 64, GPUTextureFormat::RGBA16FLOAT, "transmittance_lut");
    transmittance_lut_view = transmittance_lut_tex.create_view();

    multiscatter_lut_tex  = create_lut_texture(32, 32, GPUTextureFormat::RGBA16FLOAT, "multiscatter_lut");
    multiscatter_lut_view = multiscatter_lut_tex.create_view();

    // 4. create linear clamp sampler
    auto s_desc           = GPUSamplerDescriptor{};
    s_desc.label          = "atmosphere_linear_sampler";
    s_desc.min_filter     = GPUFilterMode::LINEAR;
    s_desc.mag_filter     = GPUFilterMode::LINEAR;
    s_desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
    s_desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
    s_desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
    linear_sampler        = device.create_sampler(s_desc);

    // 5. bind group heap
    auto h_desc      = GPUBindGroupHeapDescriptor{};
    h_desc.page_size = 4096;
    heap             = device.create_bind_group_heap(h_desc);

    // 6. create static bind groups
    if (!multiscatter_blayouts.empty()) {
        Array<GPUBindGroupEntry, 2> entries{};
        entries.at(0).type    = GPUResourceType::TEXTURE;
        entries.at(0).binding = 0;
        entries.at(0).texture = transmittance_lut_view;

        entries.at(1).type    = GPUResourceType::SAMPLER;
        entries.at(1).binding = 1;
        entries.at(1).sampler = linear_sampler;

        auto bg_desc            = GPUBindGroupDescriptor{};
        bg_desc.heap            = heap;
        bg_desc.layout          = multiscatter_blayouts.at(0);
        bg_desc.entries         = entries;
        multiscatter_bind_group = device.create_bind_group(bg_desc);
    }

    // 8. precompute static Transmittance LUT and Multi-Scattering LUT
    precompute_static_luts();

    initialized = true;
    return true;
}

void SkyboxPass::precompute_static_luts()
{
    auto cmd_desc  = GPUCommandBufferDescriptor{};
    cmd_desc.label = "atmosphere_precompute_luts";
    cmd_desc.queue = GPUQueueType::DEFAULT;
    auto cmdbuf    = device.create_command_buffer(cmd_desc);

    // 1. Transition and render Transmittance LUT (256 x 64)
    cmdbuf.resource_barrier(state_transition(
        transmittance_lut_tex,
        undefined_texture_state(),
        color_attachment_texture_state()));

    {
        auto color_att        = GPURenderPassColorAttachment{};
        color_att.view        = transmittance_lut_view;
        color_att.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
        color_att.load_op     = GPULoadOp::CLEAR;
        color_att.store_op    = GPUStoreOp::STORE;

        auto rp_desc              = GPURenderPassDescriptor{};
        rp_desc.color_attachments = color_att;

        cmdbuf.begin_render_pass(rp_desc);
        cmdbuf.set_viewport(0.0f, 0.0f, 256.0f, 64.0f);
        cmdbuf.set_scissor_rect(0, 0, 256, 64);
        cmdbuf.set_pipeline(transmittance_pipeline);
        cmdbuf.draw(6, 1, 0, 0);
        cmdbuf.end_render_pass();
    }

    cmdbuf.resource_barrier(state_transition(
        transmittance_lut_tex,
        color_attachment_texture_state(),
        shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));

    // 2. Transition and render Multiple Scattering LUT (32 x 32)
    cmdbuf.resource_barrier(state_transition(
        multiscatter_lut_tex,
        undefined_texture_state(),
        color_attachment_texture_state()));

    {
        auto color_att        = GPURenderPassColorAttachment{};
        color_att.view        = multiscatter_lut_view;
        color_att.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
        color_att.load_op     = GPULoadOp::CLEAR;
        color_att.store_op    = GPUStoreOp::STORE;

        auto rp_desc              = GPURenderPassDescriptor{};
        rp_desc.color_attachments = color_att;

        cmdbuf.begin_render_pass(rp_desc);
        cmdbuf.set_viewport(0.0f, 0.0f, 32.0f, 32.0f);
        cmdbuf.set_scissor_rect(0, 0, 32, 32);
        cmdbuf.set_pipeline(multiscatter_pipeline);
        if (multiscatter_bind_group.valid()) {
            cmdbuf.set_bind_group(0, multiscatter_bind_group);
        }
        cmdbuf.draw(6, 1, 0, 0);
        cmdbuf.end_render_pass();
    }

    cmdbuf.resource_barrier(state_transition(
        multiscatter_lut_tex,
        color_attachment_texture_state(),
        shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));

    cmdbuf.submit();
    device.wait();
}

void SkyboxPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) heap.destroy();
    if (linear_sampler.valid()) linear_sampler.destroy();

    multiscatter_bind_group = {};

    if (multiscatter_lut_view.valid()) multiscatter_lut_view.destroy();
    if (multiscatter_lut_tex.valid()) multiscatter_lut_tex.destroy();
    multiscatter_lut_tex  = {};
    multiscatter_lut_view = {};

    if (transmittance_lut_view.valid()) transmittance_lut_view.destroy();
    if (transmittance_lut_tex.valid()) transmittance_lut_tex.destroy();
    transmittance_lut_tex  = {};
    transmittance_lut_view = {};

    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();
    blayouts.clear();

    if (sky_view_pipeline.valid()) sky_view_pipeline.destroy();
    if (sky_view_playout.valid()) sky_view_playout.destroy();
    if (sky_view_fshader.valid()) sky_view_fshader.destroy();
    if (sky_view_vshader.valid()) sky_view_vshader.destroy();
    sky_view_blayouts.clear();

    if (multiscatter_pipeline.valid()) multiscatter_pipeline.destroy();
    if (multiscatter_playout.valid()) multiscatter_playout.destroy();
    if (multiscatter_fshader.valid()) multiscatter_fshader.destroy();
    if (multiscatter_vshader.valid()) multiscatter_vshader.destroy();
    multiscatter_blayouts.clear();

    if (transmittance_pipeline.valid()) transmittance_pipeline.destroy();
    if (transmittance_playout.valid()) transmittance_playout.destroy();
    if (transmittance_fshader.valid()) transmittance_fshader.destroy();
    if (transmittance_vshader.valid()) transmittance_vshader.destroy();
    transmittance_blayouts.clear();

    initialized = false;
}

auto make_skybox_uniform(const RenderView& view, const RenderScene& scene) -> SkyboxUniform
{
    SkyboxUniform u{};
    float         exposure_scale     = (view.exposure_scale > 0.0f) ? view.exposure_scale : (1.0f / (1.2f * 8192.0f));
    Vector3       sun_dir            = Vector3(0.2f, -0.6f, -0.7f);
    Vector3       sun_color          = Vector3(1.0f, 0.98f, 0.94f);
    float         sun_intensity      = 60000.0f;
    float         sun_angular_radius = 0.004675f;
    float         halo_size          = 12.0f;
    float         halo_falloff       = 80.0f;
    float         limb_darkening     = 0.6f;

    if (!scene.directional_lights.empty()) {
        const auto& light = scene.directional_lights[0];
        if (glm::length2(light.direction) > 1e-4f) {
            sun_dir = -glm::normalize(light.direction);
        }
        sun_color          = light.color;
        sun_intensity      = light.intensity;
        sun_angular_radius = light.angular_radius;
        halo_size          = light.halo_size;
        halo_falloff       = light.halo_falloff;
        limb_darkening     = light.limb_darkening;
    }

    u.proj                         = view.projection;
    u.view                         = view.view;
    u.inv_proj                     = view.inv_projection;
    u.inv_view                     = view.inv_view;
    u.camera_pos_and_sun_intensity = Vector4(view.camera_position, sun_intensity);
    u.sun_dir_and_angular_radius   = Vector4(sun_dir, sun_angular_radius);
    u.sun_color_and_exposure_scale = Vector4(sun_color, exposure_scale);
    u.sky_params                   = Vector4(halo_size, halo_falloff, limb_darkening, 0.8f);
    u.ground_albedo                = Vector4(0.12f, 0.12f, 0.14f, 1.0f);
    return u;
}

SkyboxData SkyboxPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> color_target,
    FrameGraphHandle<FrameGraphTexture> depth_target,
    const RenderView&                   view,
    const RenderScene&                  scene,
    GPULoadOp                           depth_load_op,
    GPULoadOp                           color_load_op)
{
    auto skybox_uniform = make_skybox_uniform(view, scene);

    // 1. Dynamic Sky-View LUT pass (192 x 108)
    struct SkyViewData
    {
        FrameGraphHandle<FrameGraphTexture> lut_target;
    };

    auto sky_view_data = builder.add_pass<SkyViewData>(
        "SkyViewLUTPass",
        [&](SkyViewData& data, FrameGraphPassBuilder& pass_builder) {
            auto lut_desc        = GPUTextureDescriptor{};
            lut_desc.label       = "sky_view_lut";
            lut_desc.size.width  = 192;
            lut_desc.size.height = 108;
            lut_desc.size.depth  = 1;
            lut_desc.dimension   = GPUTextureDimension::x2D;
            lut_desc.format      = GPUTextureFormat::RGBA16FLOAT;
            lut_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.lut_target      = pass_builder.create<FrameGraphTexture>(lut_desc);
            data.lut_target      = pass_builder.render(data.lut_target);
        },
        [this, skybox_uniform](const SkyViewData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !sky_view_pipeline.valid() || sky_view_blayouts.empty()) {
                return;
            }
            auto lut_tex = resources.get<FrameGraphTexture>(data.lut_target);
            if (!lut_tex) return;

            auto u_slice = ctx->create_uniform(skybox_uniform);

            Array<GPUBindGroupEntry, 4> entries{};
            entries.at(0).type    = GPUResourceType::BUFFER;
            entries.at(0).binding = 0;
            entries.at(0).buffer  = u_slice;

            entries.at(1).type    = GPUResourceType::TEXTURE;
            entries.at(1).binding = 1;
            entries.at(1).texture = transmittance_lut_view;

            entries.at(2).type    = GPUResourceType::TEXTURE;
            entries.at(2).binding = 2;
            entries.at(2).texture = multiscatter_lut_view;

            entries.at(3).type    = GPUResourceType::SAMPLER;
            entries.at(3).binding = 3;
            entries.at(3).sampler = linear_sampler;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.layout  = sky_view_blayouts.at(0);
            bg_desc.entries = entries;
            auto bg         = ctx->allocate(bg_desc);

            auto color_att        = GPURenderPassColorAttachment{};
            color_att.view        = lut_tex->view;
            color_att.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
            color_att.load_op     = GPULoadOp::CLEAR;
            color_att.store_op    = GPUStoreOp::STORE;

            auto desc              = GPURenderPassDescriptor{};
            desc.color_attachments = color_att;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, 192.0f, 108.0f);
            command.set_scissor_rect(0, 0, 192, 108);
            command.set_pipeline(sky_view_pipeline);
            if (bg.valid()) {
                command.set_bind_group(0, bg);
            }
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });

    // 2. Screen Skybox pass
    return builder.add_pass<SkyboxData>(
        "SkyboxPass",
        [&](SkyboxData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_output   = pass_builder.render(color_target);
            data.depth_input    = pass_builder.render(depth_target);
            data.sky_view_input = pass_builder.sample(sky_view_data.lut_target);
        },
        [this, skybox_uniform, depth_load_op, color_load_op, clear_color = scene.clear_color, extent = view.render_extent](const SkyboxData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid() || blayouts.empty()) {
                return;
            }

            auto color_tex    = resources.get<FrameGraphTexture>(data.color_output);
            auto depth_tex    = resources.get<FrameGraphTexture>(data.depth_input);
            auto sky_view_tex = resources.get<FrameGraphTexture>(data.sky_view_input);
            if (!color_tex || !depth_tex || !sky_view_tex) {
                return;
            }

            auto u_slice = ctx->create_uniform(skybox_uniform);

            Array<GPUBindGroupEntry, 4> entries{};
            entries.at(0).type    = GPUResourceType::BUFFER;
            entries.at(0).binding = 0;
            entries.at(0).buffer  = u_slice;

            entries.at(1).type    = GPUResourceType::TEXTURE;
            entries.at(1).binding = 1;
            entries.at(1).texture = sky_view_tex->view;

            entries.at(2).type    = GPUResourceType::TEXTURE;
            entries.at(2).binding = 2;
            entries.at(2).texture = transmittance_lut_view;

            entries.at(3).type    = GPUResourceType::SAMPLER;
            entries.at(3).binding = 3;
            entries.at(3).sampler = linear_sampler;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.layout  = blayouts.at(0);
            bg_desc.entries = entries;
            auto bg         = ctx->allocate(bg_desc);

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = color_tex->view;
            color_attachment.clear_value = GPUColor{clear_color.r, clear_color.g, clear_color.b, clear_color.a};
            color_attachment.load_op     = color_load_op;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = depth_load_op;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;
            depth_attachment.depth_read_only   = (depth_load_op == GPULoadOp::LOAD);

            auto desc                     = GPURenderPassDescriptor{};
            desc.color_attachments        = color_attachment;
            desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bg);
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });
}
