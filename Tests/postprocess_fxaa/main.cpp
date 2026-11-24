#include "helper.h"
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/FXAAPass.h>

/**
 * @file main.cpp
 * @brief RHI validation test for Fast Approximate Anti-Aliasing (FXAAPass).
 *
 * Verifies high-contrast edge detection and sub-pixel blending across sharp diagonal
 * and geometric stair-stepped edges.
 */

using namespace lyra;
using namespace lyra::passes;

namespace
{
    CString fxaa_scene_program = R"""(
struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    static const float2 quad_vertices[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0,  1.0),
    };

    float2 p = quad_vertices[vertex_id];
    VertexOutput output;
    output.position = float4(p, 0.0, 1.0);
    output.uv       = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    return output;
}

float2 rotate2d(float2 p, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float2 uv  = input.uv;
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y      = -ndc.y;

    float3 color = float3(0.12, 0.12, 0.14); // dark backdrop

    // 1. Left: White box rotated by 25 deg at (-0.55, 0.25)
    float2 p_left = rotate2d(ndc - float2(-0.55, 0.25), -0.436);
    if (abs(p_left.x) <= 0.22 && abs(p_left.y) <= 0.22) {
        color = float3(0.95, 0.95, 0.95);
    }
    // 2. Center: Cyan box rotated by 45 deg at (0.0, 0.25)
    float2 p_mid = rotate2d(ndc - float2(0.0, 0.25), -0.785);
    if (abs(p_mid.x) <= 0.20 && abs(p_mid.y) <= 0.20) {
        color = float3(0.0, 0.85, 0.95);
    }
    // 3. Right: Magenta box rotated by 65 deg at (0.55, 0.25)
    float2 p_right = rotate2d(ndc - float2(0.55, 0.25), -1.134);
    if (abs(p_right.x) <= 0.22 && abs(p_right.y) <= 0.22) {
        color = float3(0.95, 0.15, 0.45);
    }

    // 4. Bottom region: 4 sharp diagonal test bars at distinct angles
    // Strip 1: ~15 deg
    if (abs(ndc.y - (-0.45 + (ndc.x + 0.7) * 0.268)) < 0.025 && ndc.x >= -0.9 && ndc.x <= -0.5) {
        color = float3(1.0, 1.0, 1.0);
    }
    // Strip 2: ~30 deg
    if (abs(ndc.y - (-0.55 + (ndc.x + 0.25) * 0.577)) < 0.025 && ndc.x >= -0.45 && ndc.x <= -0.05) {
        color = float3(1.0, 0.9, 0.2);
    }
    // Strip 3: 45 deg
    if (abs(ndc.y - (-0.55 + (ndc.x - 0.25) * 1.0)) < 0.025 && ndc.x >= 0.05 && ndc.x <= 0.45) {
        color = float3(0.2, 1.0, 0.4);
    }
    // Strip 4: ~60 deg
    if (abs(ndc.y - (-0.55 + (ndc.x - 0.7) * 1.732)) < 0.025 && ndc.x >= 0.55 && ndc.x <= 0.85) {
        color = float3(1.0, 0.4, 0.2);
    }

    return float4(color, 1.0);
}
)""";

    struct ScenePassData
    {
        FrameGraphHandle<FrameGraphTexture> color;
    };
} // namespace

struct PostprocessFXAAApp : public TestApp
{
    FXAAPass             fxaa_pass;
    SimpleRenderPipeline scene_pipeline;
    FrameGraphAllocator  allocator;
    FrameArena           arena;

    explicit PostprocessFXAAApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();

        // 1. Setup scene generator pipeline
        auto module = execute([&]() {
            auto sdesc   = CompileDescriptor{};
            sdesc.module = "fxaa_scene_generator";
            sdesc.path   = "fxaa_scene_generator.slang";
            sdesc.source = fxaa_scene_program;
            return compiler->compile(sdesc);
        });
        REQUIRE(module != nullptr);

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });
        REQUIRE(reflection != nullptr);

        scene_pipeline.init_color_state(GPUTextureFormat::RGBA8UNORM);
        scene_pipeline.init_vshader(device, module.get(), "vsmain");
        scene_pipeline.init_fshader(device, module.get(), "fsmain");
        scene_pipeline.init_playout(device, reflection.get());
        scene_pipeline.init_pipeline(device, reflection.get());
        REQUIRE(scene_pipeline.pipeline.valid());

        // 2. Initialize FXAA pass
        REQUIRE(fxaa_pass.init(device, *compiler, get_backbuffer_format()));
    }

    ~PostprocessFXAAApp() override
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();
        device.wait();

        if (scene_pipeline.pipeline.valid()) scene_pipeline.pipeline.destroy();
        if (scene_pipeline.playout.valid()) scene_pipeline.playout.destroy();
        if (scene_pipeline.vshader.valid()) scene_pipeline.vshader.destroy();
        if (scene_pipeline.fshader.valid()) scene_pipeline.fshader.destroy();

        fxaa_pass.destroy();
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
        GPUExtent2D       extent{desc.width, desc.height};

        // 1. Generate high-contrast aliased test scene
        auto scene_data = builder.add_pass<ScenePassData>(
            "generate_aliased_scene",
            [&](ScenePassData& data, FrameGraphPassBuilder& pass_builder) {
                auto cdesc            = GPUTextureDescriptor{};
                cdesc.label           = "aliased_color_texture";
                cdesc.size.width      = extent.width;
                cdesc.size.height     = extent.height;
                cdesc.size.depth      = 1;
                cdesc.dimension       = GPUTextureDimension::x2D;
                cdesc.format          = GPUTextureFormat::RGBA8UNORM;
                cdesc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                cdesc.mip_level_count = 1;
                data.color            = pass_builder.create<FrameGraphTexture>(cdesc);
                data.color            = pass_builder.render(data.color);
            },
            [this, extent](const ScenePassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid()) return;

                auto color_tex = resources.get<FrameGraphTexture>(data.color);
                if (!color_tex) return;

                auto color_att        = GPURenderPassColorAttachment{};
                color_att.view        = color_tex->view;
                color_att.clear_value = GPUColor{0.12f, 0.12f, 0.14f, 1.0f};
                color_att.load_op     = GPULoadOp::CLEAR;
                color_att.store_op    = GPUStoreOp::STORE;

                auto pass_desc              = GPURenderPassDescriptor{};
                pass_desc.color_attachments = color_att;

                auto cmd = ctx->cmdlist;
                cmd.begin_render_pass(pass_desc);
                cmd.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
                cmd.set_scissor_rect(0, 0, extent.width, extent.height);
                cmd.set_pipeline(scene_pipeline.pipeline);
                cmd.draw(6, 1, 0, 0);
                cmd.end_render_pass();
            });

        // 2. FXAA anti-aliasing pass writing into backbuffer
        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        FXAASettings fxaa_settings{
            .subpixel_quality   = 0.75f,
            .edge_threshold     = 0.166f,
            .edge_threshold_min = 0.0833f,
            .enabled            = true,
        };

        auto fxaa_res = fxaa_pass.add(
            builder,
            scene_data.color,
            extent,
            get_backbuffer_format(),
            fxaa_settings,
            color_target);

        // 3. Present
        builder.add_pass(
            "present_pass",
            [&](FrameGraphPassBuilder& pass) {
                (void)pass.read(fxaa_res.color_output, FrameGraphReadOp::NOP);
                pass.preserve();
            },
            [this, out_h = fxaa_res.color_output](FrameGraphResources& res, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid()) return;
                auto tex = res.get<FrameGraphTexture>(out_h);
                if (tex) {
                    postprocessing(ctx->cmdlist, tex->texture);
                }
            });

        auto graph      = builder.build();
        auto fg_context = FrameGraphContext{device, swp, command, &arena};
        graph->execute(&fg_context, &allocator);

        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::postprocess_fxaa" * doctest::description("Fast approximate anti-aliasing (FXAA)"))
{
    TestAppDescriptor desc{};
    desc.name           = "vulkan";
    desc.window         = false;
    desc.backend        = RHIBackend::VULKAN;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::SPIRV;
    desc.compile_flags  = CompileFlag::DEBUG;
    PostprocessFXAAApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::postprocess_fxaa" * doctest::description("Fast approximate anti-aliasing (FXAA)"))
{
    TestAppDescriptor desc{};
    desc.name           = "d3d12";
    desc.window         = false;
    desc.backend        = RHIBackend::D3D12;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::DXIL;
    desc.compile_flags  = CompileFlag::DEBUG;
    PostprocessFXAAApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::postprocess_fxaa" * doctest::description("Fast approximate anti-aliasing (FXAA)"))
{
    TestAppDescriptor desc{};
    desc.name           = "metal";
    desc.window         = false;
    desc.backend        = RHIBackend::METAL;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::MSL;
    desc.compile_flags  = CompileFlag::DEBUG;
    PostprocessFXAAApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::postprocess_fxaa" * doctest::description("Fast approximate anti-aliasing (FXAA)"))
{
    TestAppDescriptor desc{};
    desc.name           = "softgpu";
    desc.window         = false;
    desc.backend        = RHIBackend::SOFTGPU;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG;
    desc.compile_target = CompileTarget::HOST;
    desc.compile_flags  = CompileFlag::DEBUG;
    PostprocessFXAAApp(desc).run();
}
