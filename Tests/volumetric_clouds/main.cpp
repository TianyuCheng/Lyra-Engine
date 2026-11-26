#include "helper.h"
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/SkyboxPass.h>
#include <Lyra/Rendering/Passes/VolumetricCloudPass.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace lyra;
using namespace lyra::passes;

struct VolumetricCloudsApp : public TestApp
{
    passes::SkyboxPass          skybox_pass;
    passes::VolumetricCloudPass cloud_pass;
    FrameGraph::Allocator       fg_allocator;
    FrameArena                  arena;

    explicit VolumetricCloudsApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (desc.backend == RHIBackend::SOFTGPU) {
            skipped     = true;
            skip_reason = "Volumetric cloud procedural raymarching skipped on SoftGPU to keep regression test duration fast";
            return;
        }

        if (skipped) return;

        auto& device = RHI::get_current_device();

        REQUIRE(skybox_pass.init(device, *compiler, get_backbuffer_format(), GPUTextureFormat::DEPTH32FLOAT));
        REQUIRE(cloud_pass.init(device, *compiler, get_backbuffer_format()));
    }

    ~VolumetricCloudsApp() override
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();
        device.wait();
        cloud_pass.destroy();
        skybox_pass.destroy();
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        if (skipped) return;
        auto& device = RHI::get_current_device();

        auto command = execute([&]() {
            auto cmd_desc  = GPUCommandBufferDescriptor{};
            cmd_desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(cmd_desc);
        });

        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        FrameGraphBuilder builder;

        // 1. Backbuffer import
        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        // 2. Depth buffer allocation
        struct DepthPassData
        {
            FrameGraphHandle<FrameGraphTexture> depth;
        };
        auto depth_alloc = builder.add_pass<DepthPassData>(
            "DepthAllocPass",
            [&](DepthPassData& data, FrameGraphPassBuilder& pass_builder) {
                auto depth_desc   = GPUTextureDescriptor{};
                depth_desc.label  = "clouds_depth_buffer";
                depth_desc.size   = {desc.width, desc.height, 1};
                depth_desc.format = GPUTextureFormat::DEPTH32FLOAT;
                depth_desc.usage  = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                data.depth        = pass_builder.create<FrameGraphTexture>(depth_desc);
            },
            [](const DepthPassData&, FrameGraphResources&, FrameGraphContext*) {
            });

        // 3. Setup scene view and lighting
        // Camera on ground looking slightly upward into the sunset horizon (elevation ~ 20 deg)
        RenderView view{};
        view.camera_position = Vector3(0.0f, 10.0f, 0.0f);
        Vector3 target_pos   = Vector3(0.0f, 25.0f, 60.0f);
        view.view            = glm::lookAt(view.camera_position, target_pos, Vector3(0.0f, 1.0f, 0.0f));
        view.projection      = glm::perspective(glm::radians(55.0f), float(desc.width) / float(desc.height), 0.1f, 1000.0f);
        view.inv_view        = glm::inverse(view.view);
        view.inv_projection  = glm::inverse(view.projection);
        view.render_extent   = {desc.width, desc.height};

        // Directional sun at ~15 degrees elevation shining towards camera for dramatic forward scattering
        RenderScene scene{};
        RenderLight sun_light{};
        sun_light.type           = RenderLightType::DIRECTIONAL;
        sun_light.direction      = glm::normalize(Vector3(0.12f, -0.26f, 0.95f));
        sun_light.color          = Vector3(1.0f, 0.92f, 0.82f);
        sun_light.intensity      = 35000.0f;
        sun_light.angular_radius = 0.006f;
        sun_light.halo_size      = 16.0f;
        sun_light.halo_falloff   = 60.0f;
        sun_light.limb_darkening = 0.6f;
        scene.directional_lights.push_back(sun_light);

        // 4. Atmospheric skybox pass
        auto sky_data = skybox_pass.add(
            builder,
            color_target,
            depth_alloc.depth,
            view,
            scene,
            GPULoadOp::CLEAR,
            GPULoadOp::CLEAR);

        // 5. Volumetric cloud pass
        VolumetricCloudPassParams cloud_params{};
        cloud_params.color_target         = sky_data.color_output;
        cloud_params.depth_target         = sky_data.depth_input;
        cloud_params.view                 = view;
        cloud_params.scene                = scene;
        cloud_params.time                 = 1.5f; // deterministic simulation keyframe
        cloud_params.bottom_altitude      = 1.5f;
        cloud_params.top_altitude         = 4.2f;
        cloud_params.planet_radius        = 6360.0f;
        cloud_params.max_march_distance   = 45.0f;
        cloud_params.coverage           = 0.48f;
        cloud_params.density_scale      = 0.55f;
        cloud_params.cloud_type           = 0.75f; // cumulus heavy
        cloud_params.shape_scale          = 0.35f;
        cloud_params.detail_scale         = 2.8f;
        cloud_params.detail_strength      = 0.35f;
        cloud_params.curl_strength        = 0.15f;
        cloud_params.wind_direction       = Vector3(1.0f, 0.0f, 0.3f);
        cloud_params.wind_speed           = 0.04f;
        cloud_params.height_shear         = 0.3f;
        cloud_params.multi_scatter_boost  = 0.5f;
        cloud_params.powder_scale         = 1.5f;
        cloud_params.phase_g_forward      = 0.82f;
        cloud_params.phase_g_backward     = -0.22f;
        cloud_params.phase_blend          = 0.72f;
        cloud_params.sun_color            = Vector3(1.0f, 0.94f, 0.85f);
        cloud_params.sun_intensity        = 24.0f;
        cloud_params.ambient_sky_color    = Vector3(0.35f, 0.45f, 0.65f);
        cloud_params.ambient_ground_color = Vector3(0.12f, 0.11f, 0.09f);
        cloud_params.ambient_intensity    = 1.2f;
        cloud_params.max_steps            = 28;
        cloud_params.enable_tonemap       = true;
        cloud_params.color_load_op        = GPULoadOp::LOAD;

        auto cloud_data = cloud_pass.add(builder, cloud_params);

        // 6. Presentation pass
        builder.add_pass(
            "present_pass",
            [&](FrameGraphPassBuilder& pass) {
                (void)pass.read(cloud_data.color_output, FrameGraphReadOp::NOP);
                pass.preserve();
            },
            [this, out_h = cloud_data.color_output](FrameGraphResources& res, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid()) return;
                auto tex = res.get<FrameGraphTexture>(out_h);
                if (tex) {
                    postprocessing(ctx->cmdlist, tex->texture);
                }
            });

        auto graph      = builder.build();
        auto fg_context = FrameGraphContext{device, swp, command, &arena};
        graph->execute(&fg_context, &fg_allocator);

        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::volumetric_clouds" * doctest::description("Physical volumetric cloudscape raymarching with dual-lobe phase and powder effect"))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    VolumetricCloudsApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::volumetric_clouds" * doctest::description("Physical volumetric cloudscape raymarching with dual-lobe phase and powder effect"))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    VolumetricCloudsApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::volumetric_clouds" * doctest::description("Physical volumetric cloudscape raymarching with dual-lobe phase and powder effect"))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    VolumetricCloudsApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::volumetric_clouds" * doctest::description("Physical volumetric cloudscape raymarching with dual-lobe phase and powder effect"))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    VolumetricCloudsApp(desc).run();
}
