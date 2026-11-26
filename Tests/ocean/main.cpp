#include "helper.h"
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/SkyboxPass.h>
#include <Lyra/Rendering/Passes/OceanPass.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace lyra;
using namespace lyra::passes;

struct OceanApp : public TestApp
{
    passes::SkyboxPass skybox_pass;
    passes::OceanPass  ocean_pass;
    FrameGraph::Allocator fg_allocator;
    FrameArena            arena;

    explicit OceanApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();

        REQUIRE(skybox_pass.init(device, *compiler, get_backbuffer_format(), GPUTextureFormat::DEPTH32FLOAT));
        REQUIRE(ocean_pass.init(device, *compiler, get_backbuffer_format(), GPUTextureFormat::DEPTH32FLOAT, 128, 128, 60.0f, 60.0f));
    }

    ~OceanApp() override
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();
        device.wait();
        ocean_pass.destroy();
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
                depth_desc.label  = "ocean_depth_buffer";
                depth_desc.size   = {desc.width, desc.height, 1};
                depth_desc.format = GPUTextureFormat::DEPTH32FLOAT;
                depth_desc.usage  = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                data.depth        = pass_builder.create<FrameGraphTexture>(depth_desc);
            },
            [](const DepthPassData&, FrameGraphResources&, FrameGraphContext*) {
            });

        // 3. Setup scene view and lighting
        RenderView view{};
        view.camera_position = Vector3(0.0f, 3.2f, 7.5f);
        Vector3 target_pos   = Vector3(0.0f, 0.4f, -4.0f);
        view.view            = glm::lookAt(view.camera_position, target_pos, Vector3(0.0f, 1.0f, 0.0f));
        view.projection      = glm::perspective(glm::radians(52.0f), float(desc.width) / float(desc.height), 0.1f, 1000.0f);
        view.inv_view        = glm::inverse(view.view);
        view.inv_projection  = glm::inverse(view.projection);
        view.render_extent   = {desc.width, desc.height};

        RenderScene scene{};
        RenderLight sun_light{};
        sun_light.type           = RenderLightType::DIRECTIONAL;
        sun_light.direction      = glm::normalize(Vector3(0.18f, -0.42f, -0.88f));
        sun_light.color          = Vector3(1.0f, 0.97f, 0.92f);
        sun_light.intensity      = 45000.0f;
        sun_light.angular_radius = 0.005f;
        sun_light.halo_size      = 14.0f;
        sun_light.halo_falloff   = 70.0f;
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

        // 5. Ocean surface rendering pass
        OceanPassParams ocean_params{};
        ocean_params.color_target   = sky_data.color_output;
        ocean_params.depth_target   = sky_data.depth_input;
        ocean_params.view           = view;
        ocean_params.scene          = scene;
        ocean_params.time           = 2.4f; // deterministic keyframe for visual regression
        ocean_params.water_depth    = 18.0f;
        ocean_params.water_clarity  = 1.0f;
        ocean_params.wave_speed     = 1.0f;
        ocean_params.roughness      = 0.05f;
        ocean_params.foam_scale     = 1.25f;
        ocean_params.deep_color     = Vector4(0.012f, 0.055f, 0.13f, 1.0f);
        ocean_params.shallow_color  = Vector4(0.045f, 0.28f, 0.36f, 1.0f);
        ocean_params.color_load_op  = GPULoadOp::LOAD;
        ocean_params.depth_load_op  = GPULoadOp::LOAD;

        auto ocean_data = ocean_pass.add(builder, ocean_params);

        // 6. Presentation pass
        builder.add_pass(
            "present_pass",
            [&](FrameGraphPassBuilder& pass) {
                (void)pass.read(ocean_data.color_output, FrameGraphReadOp::NOP);
                pass.preserve();
            },
            [this, out_h = ocean_data.color_output](FrameGraphResources& res, FrameGraphContext* ctx) {
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
TEST_CASE("rhi::vulkan::ocean" * doctest::description("Specialized Gerstner ocean surface renderer with foam and sun glint"))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    OceanApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::ocean" * doctest::description("Specialized Gerstner ocean surface renderer with foam and sun glint"))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    OceanApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::ocean" * doctest::description("Specialized Gerstner ocean surface renderer with foam and sun glint"))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    OceanApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::ocean" * doctest::description("Specialized Gerstner ocean surface renderer with foam and sun glint"))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    OceanApp(desc).run();
}
