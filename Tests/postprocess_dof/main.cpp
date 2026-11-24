#include "helper.h"
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Passes/DepthOfFieldPass.h>
#include <Lyra/Rendering/Passes/TonemappingPass.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    CString dof_scene_program = R"""(
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

struct SceneOutput
{
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

[shader("fragment")]
SceneOutput fsmain(VertexOutput input)
{
    float2 uv = input.uv;
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float3 color = float3(0.06, 0.07, 0.09); // dark space backdrop
    float depth = 1.0;                        // far plane (100.0m)

    // Hardware depth formula: hw_depth = (far / (far - near)) * (1.0 - near / z)
    // with near = 0.1, far = 100.0: hw_depth = (100.0 / 99.9) * (1.0 - 0.1 / z)

    // 1. Foreground Card (z = 2.0m, near field): Red with sharp vertical white stripes
    if (ndc.x >= -0.85 && ndc.x <= -0.35 && abs(ndc.y) <= 0.65) {
        float stripes = sin((ndc.x + 0.6) * 55.0);
        color = stripes > 0.0 ? float3(0.95, 0.15, 0.15) : float3(1.0, 1.0, 1.0);
        depth = (100.0 / 99.9) * (1.0 - 0.1 / 2.0);
    }
    // 2. In-Focus Plane Card (z = 5.0m = focal_distance): Emerald Green with crisp sharp grid
    else if (abs(ndc.x) <= 0.25 && abs(ndc.y) <= 0.65) {
        bool grid_x = fmod(abs(ndc.x) * 20.0, 1.0) < 0.15;
        bool grid_y = fmod(abs(ndc.y) * 20.0, 1.0) < 0.15;
        color = (grid_x || grid_y) ? float3(1.0, 1.0, 1.0) : float3(0.05, 0.85, 0.35);
        depth = (100.0 / 99.9) * (1.0 - 0.1 / 5.0);
    }
    // 3. Background Card (z = 20.0m, far field): Electric Blue with diagonal white stripes
    else if (ndc.x >= 0.35 && ndc.x <= 0.85 && abs(ndc.y) <= 0.65) {
        float diag = sin((ndc.x + ndc.y) * 40.0);
        color = diag > 0.0 ? float3(0.15, 0.35, 0.95) : float3(1.0, 1.0, 1.0);
        depth = (100.0 / 99.9) * (1.0 - 0.1 / 20.0);
    }

    SceneOutput out;
    out.color = float4(color, 1.0);
    out.depth = depth;
    return out;
}
)""";

    struct SceneGenData
    {
        FrameGraphHandle<FrameGraphTexture> color;
        FrameGraphHandle<FrameGraphTexture> depth;
    };
} // namespace

struct PostprocessDoFApp : public TestApp
{
    DepthOfFieldPass     dof_pass;
    TonemappingPass      tonemap_pass;
    SimpleRenderPipeline scene_pipeline;
    FrameGraphAllocator  allocator;
    FrameArena           arena;

    explicit PostprocessDoFApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();

        // 1. Setup scene generator pipeline
        auto module = execute([&]() {
            auto sdesc   = CompileDescriptor{};
            sdesc.module = "dof_scene_generator";
            sdesc.path   = "dof_scene_generator.slang";
            sdesc.source = dof_scene_program;
            return compiler->compile(sdesc);
        });
        REQUIRE(module != nullptr);

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });
        REQUIRE(reflection != nullptr);

        scene_pipeline.init_color_state(GPUTextureFormat::RGBA16FLOAT);
        scene_pipeline.init_depth_state(GPUTextureFormat::DEPTH32FLOAT, GPUCompareFunction::ALWAYS, true);
        scene_pipeline.init_vshader(device, module.get(), "vsmain");
        scene_pipeline.init_fshader(device, module.get(), "fsmain");
        scene_pipeline.init_playout(device, reflection.get());
        scene_pipeline.init_pipeline(device, reflection.get());
        REQUIRE(scene_pipeline.pipeline.valid());

        // 2. Initialize DOF and tonemapping passes
        REQUIRE(dof_pass.init(device, *compiler));
        REQUIRE(tonemap_pass.init(device, *compiler, get_backbuffer_format()));
    }

    ~PostprocessDoFApp()
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();
        device.wait();

        if (scene_pipeline.pipeline.valid()) scene_pipeline.pipeline.destroy();
        if (scene_pipeline.playout.valid()) scene_pipeline.playout.destroy();
        if (scene_pipeline.vshader.valid()) scene_pipeline.vshader.destroy();
        if (scene_pipeline.fshader.valid()) scene_pipeline.fshader.destroy();

        tonemap_pass.destroy();
        dof_pass.destroy();
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

        // 1. Generate Scene Color (RGBA16FLOAT) & Depth (DEPTH32FLOAT) pass
        auto scene_data = builder.add_pass<SceneGenData>(
            "SceneGenPass",
            [&](SceneGenData& data, FrameGraphPassBuilder& pass_builder) {
                auto color_desc            = GPUTextureDescriptor{};
                color_desc.label           = "scene_color";
                color_desc.size.width      = extent.width;
                color_desc.size.height     = extent.height;
                color_desc.size.depth      = 1;
                color_desc.dimension       = GPUTextureDimension::x2D;
                color_desc.format          = GPUTextureFormat::RGBA16FLOAT;
                color_desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                color_desc.mip_level_count = 1;
                data.color                 = pass_builder.create<FrameGraphTexture>(color_desc);
                data.color                 = pass_builder.render(data.color);

                auto depth_desc            = GPUTextureDescriptor{};
                depth_desc.label           = "scene_depth";
                depth_desc.size.width      = extent.width;
                depth_desc.size.height     = extent.height;
                depth_desc.size.depth      = 1;
                depth_desc.dimension       = GPUTextureDimension::x2D;
                depth_desc.format          = GPUTextureFormat::DEPTH32FLOAT;
                depth_desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                depth_desc.mip_level_count = 1;
                data.depth                 = pass_builder.create<FrameGraphTexture>(depth_desc);
                data.depth                 = pass_builder.render(data.depth);
            },
            [this, extent](const SceneGenData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid()) return;
                auto color_tex = resources.get<FrameGraphTexture>(data.color);
                auto depth_tex = resources.get<FrameGraphTexture>(data.depth);
                if (!color_tex || !depth_tex) return;

                auto color_att        = GPURenderPassColorAttachment{};
                color_att.view        = color_tex->view;
                color_att.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
                color_att.load_op     = GPULoadOp::CLEAR;
                color_att.store_op    = GPUStoreOp::STORE;

                auto depth_att              = GPURenderPassDepthStencilAttachment{};
                depth_att.view              = depth_tex->view;
                depth_att.depth_clear_value = 1.0f;
                depth_att.depth_load_op     = GPULoadOp::CLEAR;
                depth_att.depth_store_op    = GPUStoreOp::STORE;

                auto pass_desc                     = GPURenderPassDescriptor{};
                pass_desc.color_attachments        = color_att;
                pass_desc.depth_stencil_attachment = depth_att;

                auto cmd = ctx->cmdlist;
                cmd.begin_render_pass(pass_desc);
                cmd.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
                cmd.set_scissor_rect(0, 0, extent.width, extent.height);
                cmd.set_pipeline(scene_pipeline.pipeline);
                cmd.draw(6, 1, 0, 0);
                cmd.end_render_pass();
            });

        // 2. Depth of Field pass (85mm f/1.4 lens, focal distance = 5.0m)
        DOFSettings dof_settings{
            .focal_distance  = 5.0f,
            .focal_length    = 0.085f,
            .aperture_f_stop = 1.4f,
            .max_coc_radius  = 20.0f,
            .enabled         = true,
        };

        auto dof_data = dof_pass.add(
            builder,
            scene_data.color,
            scene_data.depth,
            extent,
            0.1f,
            100.0f,
            dof_settings);

        // 3. Tonemapping Pass mapping HDR DOF output to LDR display target
        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        TonemappingSettings tonemap_settings{
            .exposure          = 1.0f,
            .operator_type     = 0, // ACES
            .bloom_intensity   = 0.0f,
            .use_auto_exposure = false,
        };

        auto tonemap_data = tonemap_pass.add(
            builder,
            dof_data.dof_output,
            extent,
            {},
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
TEST_CASE("rhi::vulkan::postprocess_dof" * doctest::description("Physical depth of field with circle of confusion bokeh"))
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
    PostprocessDoFApp(desc).run();
}
#endif

#if defined(_WIN32)
TEST_CASE("rhi::d3d12::postprocess_dof" * doctest::description("Physical depth of field with circle of confusion bokeh"))
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
    PostprocessDoFApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::postprocess_dof" * doctest::description("Physical depth of field with circle of confusion bokeh"))
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
    PostprocessDoFApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::postprocess_dof" * doctest::description("Physical depth of field with circle of confusion bokeh"))
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
    desc.features       = {GPUFeatureName::BGRA8UNORM_STORAGE};
    PostprocessDoFApp(desc).run();
}
