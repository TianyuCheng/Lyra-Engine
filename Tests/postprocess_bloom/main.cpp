#include "helper.h"
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/BloomPass.h>
#include <Lyra/Rendering/Passes/TonemappingPass.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    CString bloom_scene_program = R"""(
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
        float2(-1.0,  1.0)
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
    float2 uv = input.uv;
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y; // NDC Y positive upwards

    // 1. Dark space backdrop
    float3 color = float3(0.04, 0.045, 0.06);

    // 2. Dim reference pedestal at bottom: y in [-0.70, -0.50], x in [-0.75, 0.75]
    // Luminance = 0.35 < 1.0 (threshold), so it must NOT trigger bloom
    if (ndc.y >= -0.70 && ndc.y <= -0.50 && abs(ndc.x) <= 0.75) {
        color = float3(0.32, 0.35, 0.38);
    }

    // 3. Three bright emissive neon discs (Luminance >> 1.0, triggering strong bloom halos)
    // Left Disc (Neon Cyan): center (-0.50, 0.05), radius 0.20
    float d_cyan = length(ndc - float2(-0.50, 0.05));
    if (d_cyan <= 0.20) {
        color = float3(0.0, 15.0, 25.0);
    }

    // Center Disc (Radiant Magenta): center (0.00, 0.05), radius 0.20
    float d_mag = length(ndc - float2(0.00, 0.05));
    if (d_mag <= 0.20) {
        color = float3(25.0, 0.0, 15.0);
    }

    // Right Disc (Electric Gold): center (0.50, 0.05), radius 0.20
    float d_gold = length(ndc - float2(0.50, 0.05));
    if (d_gold <= 0.20) {
        color = float3(25.0, 20.0, 0.0);
    }

    return float4(color, 1.0);
}
)""";

    struct SceneGenData
    {
        FrameGraphHandle<FrameGraphTexture> hdr_color;
    };
} // namespace

struct PostprocessBloomApp : public TestApp
{
    BloomPass            bloom_pass;
    TonemappingPass      tonemap_pass;
    SimpleRenderPipeline scene_pipeline;
    FrameGraphAllocator  allocator;
    FrameArena           arena;

    explicit PostprocessBloomApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();

        // 1. setup scene generator pipeline
        auto module = execute([&]() {
            auto sdesc   = CompileDescriptor{};
            sdesc.module = "bloom_scene_generator";
            sdesc.path   = "bloom_scene_generator.slang";
            sdesc.source = bloom_scene_program;
            return compiler->compile(sdesc);
        });
        REQUIRE(module != nullptr);

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });
        REQUIRE(reflection != nullptr);

        scene_pipeline.init_color_state(GPUTextureFormat::RGBA16FLOAT);
        scene_pipeline.init_vshader(device, module->get(), "vsmain");
        scene_pipeline.init_fshader(device, module->get(), "fsmain");
        scene_pipeline.init_playout(device, reflection->get());
        scene_pipeline.init_pipeline(device, reflection->get());
        REQUIRE(scene_pipeline.pipeline.valid());

        // 2. initialize passes
        REQUIRE(bloom_pass.init(device, *compiler));
        REQUIRE(tonemap_pass.init(device, *compiler, get_backbuffer_format()));
    }

    ~PostprocessBloomApp()
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();
        device.wait();

        if (scene_pipeline.pipeline.valid()) scene_pipeline.pipeline.destroy();
        if (scene_pipeline.playout.valid()) scene_pipeline.playout.destroy();
        if (scene_pipeline.vshader.valid()) scene_pipeline.vshader.destroy();
        if (scene_pipeline.fshader.valid()) scene_pipeline.fshader.destroy();

        bloom_pass.destroy();
        tonemap_pass.destroy();
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        if (skipped) return;

        auto& device  = RHI::get_current_device();
        auto  command = execute([&]() {
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

        // 1. Generate HDR Scene pass (RGBA16FLOAT)
        auto scene_data = builder.add_pass<SceneGenData>(
            "SceneGenPass",
            [&](SceneGenData& data, FrameGraphPassBuilder& pass_builder) {
                auto hdr_desc            = GPUTextureDescriptor{};
                hdr_desc.label           = "scene_hdr_color";
                hdr_desc.size.width      = extent.width;
                hdr_desc.size.height     = extent.height;
                hdr_desc.size.depth      = 1;
                hdr_desc.dimension       = GPUTextureDimension::x2D;
                hdr_desc.format          = GPUTextureFormat::RGBA16FLOAT;
                hdr_desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                hdr_desc.mip_level_count = 1;
                data.hdr_color           = pass_builder.create<FrameGraphTexture>(hdr_desc);
                data.hdr_color           = pass_builder.render(data.hdr_color);
            },
            [this, extent](const SceneGenData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid()) return;
                auto hdr_tex = resources.get<FrameGraphTexture>(data.hdr_color);
                if (!hdr_tex) return;

                auto color_att        = GPURenderPassColorAttachment{};
                color_att.view        = hdr_tex->view;
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

        // 2. Bloom pass
        BloomSettings bloom_settings{
            .threshold       = 1.0f,
            .knee            = 0.5f,
            .intensity       = 1.0f,
            .upsample_radius = 1.0f,
            .enabled         = true,
        };
        auto bloom_data = bloom_pass.add(builder, scene_data.hdr_color, extent, bloom_settings);

        // 3. Tonemapping pass (ACES tonemapping + bloom compositing into backbuffer)
        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        TonemappingSettings tonemap_settings{
            .exposure          = 1.0f,
            .operator_type     = 0, // ACES
            .bloom_intensity   = 1.0f,
            .use_auto_exposure = false,
        };

        auto tonemap_data = tonemap_pass.add(
            builder,
            scene_data.hdr_color,
            extent,
            bloom_data.bloom_output,
            {},
            tonemap_settings,
            get_backbuffer_format(),
            color_target);

        // 4. Present
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
TEST_CASE("rhi::vulkan::postprocess_bloom" * doctest::description("Dual-filtering threshold bloom"))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PostprocessBloomApp(desc).run();
}
#endif

#if defined(_WIN32)
TEST_CASE("rhi::d3d12::postprocess_bloom" * doctest::description("Dual-filtering threshold bloom"))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PostprocessBloomApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::postprocess_bloom" * doctest::description("Dual-filtering threshold bloom"))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PostprocessBloomApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::postprocess_bloom" * doctest::description("Dual-filtering threshold bloom"))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    desc.features      = {GPUFeatureName::BGRA8UNORM_STORAGE};
    PostprocessBloomApp(desc).run();
}
