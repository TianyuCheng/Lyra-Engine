#include "helper.h"

CString color_write_mask_program = R"""(
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

struct ColorWriteVertex
{
    glm::vec3 position;
    float     _pad = 0.0f;
    glm::vec4 color;
};

struct ColorWriteMaskApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    SimpleRenderPipeline pipeline_base;
    SimpleRenderPipeline pipeline_red;
    SimpleRenderPipeline pipeline_green;
    SimpleRenderPipeline pipeline_blue;
    SimpleRenderPipeline pipeline_rg;
    SimpleRenderPipeline pipeline_none;

    explicit ColorWriteMaskApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<ColorWriteVertex> vertices;
        Vector<uint32_t>         indices;

        // helper: add a quad with ccw triangles
        auto add_quad = [&](float x0, float y0, float x1, float y1, const glm::vec4& color) {
            uint32_t         base = static_cast<uint32_t>(vertices.size());
            ColorWriteVertex v0{}, v1{}, v2{}, v3{};
            v0.position = {x0, y0, 0.0f}; // bottom-left
            v0.color    = color;
            v1.position = {x1, y0, 0.0f}; // bottom-right
            v1.color    = color;
            v2.position = {x1, y1, 0.0f}; // top-right
            v2.color    = color;
            v3.position = {x0, y1, 0.0f}; // top-left
            v3.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // ccw: (0, 1, 2) and (0, 2, 3)
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        // helper: add a diamond with ccw triangles
        auto add_diamond = [&](float cx, float cy, float rw, float rh, const glm::vec4& color) {
            uint32_t         base = static_cast<uint32_t>(vertices.size());
            ColorWriteVertex v0{}, v1{}, v2{}, v3{};
            v0.position = {cx, cy + rh, 0.0f}; // top
            v0.color    = color;
            v1.position = {cx - rw, cy, 0.0f}; // left
            v1.color    = color;
            v2.position = {cx, cy - rh, 0.0f}; // bottom
            v2.color    = color;
            v3.position = {cx + rw, cy, 0.0f}; // right
            v3.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // ccw: (top, left, bottom) and (top, bottom, right)
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        const float hw = 0.36f;
        const float hh = 0.36f;
        const float rw = 0.22f;
        const float rh = 0.22f;
        const float iw = 0.09f;
        const float ih = 0.09f;

        // Base card color: dark navy slate (#1A1F29)
        const glm::vec4 card_color{0.10f, 0.12f, 0.16f, 1.0f};
        // Pure opaque white for channel-masked diamonds (#FFFFFF)
        const glm::vec4 white{1.0f, 1.0f, 1.0f, 1.0f};
        // Black color attempted to write with NONE mask (#000000)
        const glm::vec4 black{0.0f, 0.0f, 0.0f, 1.0f};

        // 1. Four dark slate cards (indices 0..23, drawn with pipeline_base: GPUColorWrite::ALL)
        // Q0 (Top-Left)
        add_quad(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh, card_color);
        // Q1 (Top-Right)
        add_quad(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, card_color);
        // Q2 (Bottom-Left)
        add_quad(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, card_color);
        // Q3 (Bottom-Right)
        add_quad(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, card_color);

        // 2. Q0 Top-Left: Large diamond (indices 24..29, drawn with pipeline_red: GPUColorWrite::RED)
        add_diamond(-0.5f, 0.5f, rw, rh, white);

        // 3. Q1 Top-Right: Large diamond (indices 30..35, drawn with pipeline_green: GPUColorWrite::GREEN)
        add_diamond(0.5f, 0.5f, rw, rh, white);

        // 4. Q2 Bottom-Left: Large diamond (indices 36..41, drawn with pipeline_blue: GPUColorWrite::BLUE)
        add_diamond(-0.5f, -0.5f, rw, rh, white);

        // 5. Q3 Bottom-Right: Large diamond (indices 42..47, drawn with pipeline_rg: GPUColorWrite::RED | GREEN)
        add_diamond(0.5f, -0.5f, rw, rh, white);

        // 6. Inner diamonds (indices 48..71, drawn with pipeline_none: GPUColorWrite::NONE)
        // Attempting to overwrite the center with black; write suppression preserves the colored diamonds.
        add_diamond(-0.5f, 0.5f, iw, ih, black);
        add_diamond(0.5f, 0.5f, iw, ih, black);
        add_diamond(-0.5f, -0.5f, iw, ih, black);
        add_diamond(0.5f, -0.5f, iw, ih, black);

        vbuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vertex_buffer";
            desc.size               = sizeof(ColorWriteVertex) * vertices.size();
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

        auto mapped_v = vbuffer.get_mapped_range<ColorWriteVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(ColorWriteVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint32_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint32_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "color_write_mask_shader";
            desc.path   = "color_write_mask.slang";
            desc.source = color_write_mask_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        auto init_pipe = [&](SimpleRenderPipeline& pipe, GPUColorWriteFlags write_mask) {
            pipe.vstride = sizeof(ColorWriteVertex);
            pipe.attributes.push_back({"position", offsetof(ColorWriteVertex, position)});
            pipe.attributes.push_back({"color", offsetof(ColorWriteVertex, color)});

            pipe.init_color_state(get_backbuffer_format(), write_mask);
            pipe.init_vshader(device, module->get(), "vsmain");
            pipe.init_fshader(device, module->get(), "fsmain");
            pipe.init_playout(device, reflection->get());
            pipe.init_pipeline(device, reflection->get());
        };

        init_pipe(pipeline_base, GPUColorWrite::ALL);
        init_pipe(pipeline_red, GPUColorWrite::RED);
        init_pipe(pipeline_green, GPUColorWrite::GREEN);
        init_pipe(pipeline_blue, GPUColorWrite::BLUE);
        init_pipe(pipeline_rg, GPUColorWrite::RED | GPUColorWrite::GREEN);
        init_pipe(pipeline_none, GPUColorWrite::NONE);
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
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);

        command.set_vertex_buffer(0, vbuffer);
        command.set_index_buffer(ibuffer, GPUIndexFormat::UINT32);

        // 1. Draw base cards (24 indices) with pipeline_base (GPUColorWrite::ALL)
        command.set_pipeline(pipeline_base.pipeline);
        command.draw_indexed(24, 1, 0, 0, 0);

        // 2. Q0 Top-Left: Large diamond (6 indices, offset 24) with pipeline_red (GPUColorWrite::RED)
        command.set_pipeline(pipeline_red.pipeline);
        command.draw_indexed(6, 1, 24, 0, 0);

        // 3. Q1 Top-Right: Large diamond (6 indices, offset 30) with pipeline_green (GPUColorWrite::GREEN)
        command.set_pipeline(pipeline_green.pipeline);
        command.draw_indexed(6, 1, 30, 0, 0);

        // 4. Q2 Bottom-Left: Large diamond (6 indices, offset 36) with pipeline_blue (GPUColorWrite::BLUE)
        command.set_pipeline(pipeline_blue.pipeline);
        command.draw_indexed(6, 1, 36, 0, 0);

        // 5. Q3 Bottom-Right: Large diamond (6 indices, offset 42) with pipeline_rg (GPUColorWrite::RED | GREEN)
        command.set_pipeline(pipeline_rg.pipeline);
        command.draw_indexed(6, 1, 42, 0, 0);

        // 6. Draw inner diamonds (24 indices, offset 48) with pipeline_none (GPUColorWrite::NONE)
        command.set_pipeline(pipeline_none.pipeline);
        command.draw_indexed(24, 1, 48, 0, 0);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::raster_color_write_mask" * doctest::description("Testing rasterizer color write channel masking."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ColorWriteMaskApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::raster_color_write_mask" * doctest::description("Testing rasterizer color write channel masking."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ColorWriteMaskApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::raster_color_write_mask" * doctest::description("Testing rasterizer color write channel masking."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ColorWriteMaskApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::raster_color_write_mask" * doctest::description("Testing rasterizer color write channel masking."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    ColorWriteMaskApp(desc).run();
}
