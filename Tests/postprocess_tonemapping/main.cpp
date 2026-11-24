#include "helper.h"
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/TonemappingPass.h>

/**
 * @file main.cpp
 * @brief RHI validation test for HDR Tonemapping & Color Reproduction (AgX curve).
 *
 * Validates HDR filmic tonemapping curve (AgX) across extreme high-dynamic-range
 * radiance values, preventing clipping and preserving hue fidelity.
 */

using namespace lyra;
using namespace lyra::passes;

namespace
{
    CString tonemap_scene_program = R"""(
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

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float2 uv  = input.uv;
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y      = -ndc.y;

    float3 color = float3(0.02, 0.025, 0.03); // neutral dark background

    // Bottom region: continuous HDR ramp (ndc.y between -0.85 and -0.45)
    if (ndc.y >= -0.85 && ndc.y <= -0.45 && abs(ndc.x) <= 0.9) {
        float t = (ndc.x + 0.9) / 1.8; // [0, 1]
        // Smooth spectral rainbow sweep up to 32.0 radiance
        float radiance = t * 32.0;
        float3 tint = float3(
            0.5 + 0.5 * cos(6.28318 * (t + 0.0)),
            0.5 + 0.5 * cos(6.28318 * (t + 0.33)),
            0.5 + 0.5 * cos(6.28318 * (t + 0.67))
        );
        color = tint * radiance;
    }
    // Top region: 4 distinct HDR discs across dynamic ranges [0.5, 35.0]
    // 1. Standard LDR: Red disc at x = -0.65, y = 0.25 (radius 0.22)
    else if (length(float2(ndc.x - (-0.65), ndc.y - 0.25)) <= 0.22) {
        color = float3(0.8, 0.15, 0.2); // unclipped LDR
    }
    // 2. Moderate HDR: Amber / Gold disc at x = -0.22, y = 0.25 (radius 0.22)
    else if (length(float2(ndc.x - (-0.22), ndc.y - 0.25)) <= 0.22) {
        color = float3(2.5, 1.2, 0.2); // 2.5x HDR
    }
    // 3. High Specular / Neon: Cyan / Violet disc at x = 0.22, y = 0.25 (radius 0.22)
    else if (length(float2(ndc.x - (0.22), ndc.y - 0.25)) <= 0.22) {
        color = float3(1.0, 4.0, 12.0); // 12x HDR
    }
    // 4. Extreme Super-Radiance: White-hot sun disc at x = 0.65, y = 0.25 (radius 0.22)
    else if (length(float2(ndc.x - (0.65), ndc.y - 0.25)) <= 0.22) {
        color = float3(35.0, 28.0, 15.0); // 35x Extreme Radiance
    }

    return float4(color, 1.0);
}
)""";

    struct ScenePassData
    {
        FrameGraphHandle<FrameGraphTexture> hdr_color;
    };
} // namespace

struct PostprocessTonemapApp : public TestApp
{
    TonemappingPass      tonemap_pass;
    SimpleRenderPipeline scene_pipeline;
    FrameGraphAllocator  allocator;
    FrameArena           arena;

    explicit PostprocessTonemapApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();

        // 1. Setup scene generator pipeline
        auto module = execute([&]() {
            auto sdesc   = CompileDescriptor{};
            sdesc.module = "tonemap_scene_generator";
            sdesc.path   = "tonemap_scene_generator.slang";
            sdesc.source = tonemap_scene_program;
            return compiler->compile(sdesc);
        });
        REQUIRE(module != nullptr);

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });
        REQUIRE(reflection != nullptr);

        scene_pipeline.init_color_state(GPUTextureFormat::RGBA16FLOAT);
        scene_pipeline.init_vshader(device, module.get(), "vsmain");
        scene_pipeline.init_fshader(device, module.get(), "fsmain");
        scene_pipeline.init_playout(device, reflection.get());
        scene_pipeline.init_pipeline(device, reflection.get());
        REQUIRE(scene_pipeline.pipeline.valid());

        // 2. Initialize Tonemapping pass
        REQUIRE(tonemap_pass.init(device, *compiler, get_backbuffer_format()));
    }

    ~PostprocessTonemapApp() override
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();
        device.wait();

        if (scene_pipeline.pipeline.valid()) scene_pipeline.pipeline.destroy();
        if (scene_pipeline.playout.valid()) scene_pipeline.playout.destroy();
        if (scene_pipeline.vshader.valid()) scene_pipeline.vshader.destroy();
        if (scene_pipeline.fshader.valid()) scene_pipeline.fshader.destroy();

        tonemap_pass.destroy();
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

        // 1. Generate HDR test scene
        auto scene_data = builder.add_pass<ScenePassData>(
            "generate_hdr_scene",
            [&](ScenePassData& data, FrameGraphPassBuilder& pass_builder) {
                auto cdesc            = GPUTextureDescriptor{};
                cdesc.label           = "hdr_color_texture";
                cdesc.size.width      = extent.width;
                cdesc.size.height     = extent.height;
                cdesc.size.depth      = 1;
                cdesc.dimension       = GPUTextureDimension::x2D;
                cdesc.format          = GPUTextureFormat::RGBA16FLOAT;
                cdesc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                cdesc.mip_level_count = 1;
                data.hdr_color        = pass_builder.create<FrameGraphTexture>(cdesc);
                data.hdr_color        = pass_builder.render(data.hdr_color);
            },
            [this, extent](const ScenePassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid()) return;

                auto color_tex = resources.get<FrameGraphTexture>(data.hdr_color);
                if (!color_tex) return;

                auto color_att        = GPURenderPassColorAttachment{};
                color_att.view        = color_tex->view;
                color_att.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
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

        // 2. Tonemapping Pass with AgX curve
        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        TonemappingSettings tonemap_settings{
            .exposure          = 1.0f,
            .operator_type     = 2, // AgX
            .bloom_intensity   = 0.0f,
            .use_auto_exposure = false,
        };

        auto tonemap_data = tonemap_pass.add(
            builder,
            scene_data.hdr_color,
            extent,
            {},
            {},
            tonemap_settings,
            get_backbuffer_format(),
            color_target);

        // 3. Present
        builder.add_pass(
            "present_pass",
            [&](FrameGraphPassBuilder& pass) {
                (void)pass.read(tonemap_data.ldr_output, FrameGraphReadOp::NOP);
                pass.preserve();
            },
            [this, out_h = tonemap_data.ldr_output](FrameGraphResources& res, FrameGraphContext* ctx) {
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
TEST_CASE("rhi::vulkan::postprocess_tonemapping" * doctest::description("HDR Tonemapping and color grading"))
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
    PostprocessTonemapApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::postprocess_tonemapping" * doctest::description("HDR Tonemapping and color grading"))
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
    PostprocessTonemapApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::postprocess_tonemapping" * doctest::description("HDR Tonemapping and color grading"))
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
    PostprocessTonemapApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::postprocess_tonemapping" * doctest::description("HDR Tonemapping and color grading"))
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
    PostprocessTonemapApp(desc).run();
}
