#include "helper.h"

CString scissor_viewport_program = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float4 color    : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return input.color;
}
)""";

struct ScissorVertex
{
    glm::vec3 position;
    float     _pad = 0.0f;
    glm::vec4 color;
};

struct ScissorViewportApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    SimpleRenderPipeline pipeline;

    explicit ScissorViewportApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<ScissorVertex> vertices;
        Vector<uint32_t>      indices;

        auto add_quad = [&](float x0, float y0, float x1, float y1, const glm::vec4& color) {
            uint32_t      base = static_cast<uint32_t>(vertices.size());
            ScissorVertex v0{}, v1{}, v2{}, v3{};
            v0.position = {x0, y0, 0.0f};
            v0.color    = color;
            v1.position = {x1, y0, 0.0f};
            v1.color    = color;
            v2.position = {x1, y1, 0.0f};
            v2.color    = color;
            v3.position = {x0, y1, 0.0f};
            v3.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        auto add_diamond = [&](float cx, float cy, float rw, float rh, const glm::vec4& color) {
            uint32_t      base = static_cast<uint32_t>(vertices.size());
            ScissorVertex v0{}, v1{}, v2{}, v3{};
            v0.position = {cx, cy + rh, 0.0f};
            v0.color    = color;
            v1.position = {cx - rw, cy, 0.0f};
            v1.color    = color;
            v2.position = {cx, cy - rh, 0.0f};
            v2.color    = color;
            v3.position = {cx + rw, cy, 0.0f};
            v3.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        // 1. Q0 (Top-Left): Base card + Inset clipped quad
        // Base navy card (indices 0..5)
        add_quad(-0.86f, 0.14f, -0.14f, 0.86f, {0.10f, 0.14f, 0.22f, 1.0f});
        // Oversized crimson quad spanning entire quadrant [-1.0, 0.0] x [0.0, 1.0] (indices 6..11)
        add_quad(-1.0f, 0.0f, 0.0f, 1.0f, {0.95f, 0.20f, 0.25f, 1.0f});

        // 2. Q1 (Top-Right): Sub-viewport geometry (in normalized sub-viewport coordinates)
        // Sub-viewport card (indices 12..17)
        add_quad(-0.90f, -0.90f, 0.90f, 0.90f, {0.08f, 0.12f, 0.16f, 1.0f});
        // Emerald diamond (indices 18..23)
        add_diamond(0.0f, 0.0f, 0.60f, 0.60f, {0.15f, 0.95f, 0.35f, 1.0f});

        // 3. Q2 (Bottom-Left): Base card + Scissor stripe
        // Warm gold card (indices 24..29)
        add_quad(-0.86f, -0.86f, -0.14f, -0.14f, {0.85f, 0.55f, 0.10f, 1.0f});
        // Oversized cyan quad (indices 30..35)
        add_quad(-0.65f, -1.0f, -0.35f, 0.0f, {0.10f, 0.90f, 1.00f, 1.0f});

        // 4. Q3 (Bottom-Right): Sub-viewport geometry with split scissor
        // Sub-viewport card (indices 36..41)
        add_quad(-0.90f, -0.90f, 0.90f, 0.90f, {0.14f, 0.08f, 0.18f, 1.0f});
        // Amber diamond (indices 42..47)
        add_diamond(0.0f, 0.0f, 0.60f, 0.60f, {1.00f, 0.75f, 0.15f, 1.0f});
        // Violet diamond (to be split by scissor) (indices 48..53)
        add_diamond(0.0f, 0.0f, 0.40f, 0.40f, {0.70f, 0.15f, 1.00f, 1.0f});

        vbuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vertex_buffer";
            desc.size               = sizeof(ScissorVertex) * vertices.size();
            desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        ibuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "index_buffer";
            desc.size               = sizeof(uint32_t) * indices.size();
            desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        auto mapped_v = vbuffer.get_mapped_range<ScissorVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(ScissorVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint32_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint32_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "scissor_viewport_shader";
            desc.path   = "scissor_viewport.slang";
            desc.source = scissor_viewport_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(ScissorVertex);
        pipeline.attributes.push_back({"position", offsetof(ScissorVertex, position)});
        pipeline.attributes.push_back({"color", offsetof(ScissorVertex, color)});

        pipeline.init_color_state(get_backbuffer_format());
        pipeline.init_vshader(device, module.get(), "vsmain");
        pipeline.init_fshader(device, module.get(), "fsmain");
        pipeline.init_playout(device, reflection.get());
        pipeline.init_pipeline(device, reflection.get());
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        auto command = execute([&]() {
            auto desc  = GPUCommandBufferDescriptor{};
            desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        auto color_attachment        = GPURenderPassColorAttachment{};
        color_attachment.clear_value = GPUColor{0.07f, 0.08f, 0.11f, 1.0f};
        color_attachment.load_op     = GPULoadOp::CLEAR;
        color_attachment.store_op    = GPUStoreOp::STORE;
        color_attachment.view        = backbuffer.view;

        auto render_pass              = GPURenderPassDescriptor{};
        render_pass.color_attachments = color_attachment;

        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), color_attachment_texture_state()));

        command.begin_render_pass(render_pass);
        command.set_pipeline(pipeline.pipeline);
        command.set_vertex_buffer(0, vbuffer);
        command.set_index_buffer(ibuffer, GPUIndexFormat::UINT32);

        // --- 1. Q0 (Top-Left): Inset Scissor Box ---
        // Draw base card with full viewport and scissor
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.draw_indexed(6, 1, 0, 0, 0);

        // Set scissor to inset window (50, 40, 220, 160) and draw oversized crimson quad
        command.set_scissor_rect(50, 40, 220, 160);
        command.draw_indexed(6, 1, 6, 0, 0);

        // --- 2. Q1 (Top-Right): Sub-Viewport Mapping ---
        command.set_viewport(340, 20, 260, 200);
        command.set_scissor_rect(340, 20, 260, 200);
        command.draw_indexed(6, 1, 12, 0, 0); // base card
        command.draw_indexed(6, 1, 18, 0, 0); // emerald diamond

        // --- 3. Q2 (Bottom-Left): Horizontal Scissor Stripe ---
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.draw_indexed(6, 1, 24, 0, 0); // warm gold card

        // Scissor stripe (45, 310, 230, 60)
        command.set_scissor_rect(45, 310, 230, 60);
        command.draw_indexed(6, 1, 30, 0, 0); // cyan quad clipped into horizontal band

        // --- 4. Q3 (Bottom-Right): Sub-Viewport with Split Scissor ---
        command.set_viewport(350, 260, 250, 200);
        command.set_scissor_rect(350, 260, 250, 200);
        command.draw_indexed(6, 1, 36, 0, 0); // plum card
        command.draw_indexed(6, 1, 42, 0, 0); // amber diamond

        // Left-half scissor within Q3 sub-viewport (350, 260, 125, 200)
        command.set_scissor_rect(350, 260, 125, 200);
        command.draw_indexed(6, 1, 48, 0, 0); // violet diamond split cleanly down the middle

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::raster_scissor_viewport" * doctest::description("Testing rasterizer scissor clipping and viewport transformations."))
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
    ScissorViewportApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::raster_scissor_viewport" * doctest::description("Testing rasterizer scissor clipping and viewport transformations."))
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
    ScissorViewportApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::raster_scissor_viewport" * doctest::description("Testing rasterizer scissor clipping and viewport transformations."))
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
    ScissorViewportApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::raster_scissor_viewport" * doctest::description("Testing rasterizer scissor clipping and viewport transformations."))
{
    TestAppDescriptor desc{};
    desc.name           = "softgpu";
    desc.window         = false;
    desc.backend        = RHIBackend::SOFTGPU;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG;
    desc.compile_target = CompileTarget::SPIRV;
    desc.compile_flags  = CompileFlag::DEBUG;
    ScissorViewportApp(desc).run();
}
