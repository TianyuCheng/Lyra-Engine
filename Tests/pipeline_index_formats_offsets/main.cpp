#include "helper.h"

CString index_formats_offsets_program = R"""(
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

struct TopoVertex
{
    glm::vec3 position;
    float     _pad = 0.0f;
    glm::vec4 color;
};

struct IndexFormatsOffsetsApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer_u16;
    GPUBuffer            ibuffer_u32;
    SimpleRenderPipeline pipeline;

    explicit IndexFormatsOffsetsApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<TopoVertex> vertices;
        Vector<uint16_t>   indices_u16;
        Vector<uint32_t>   indices_u32;

        const glm::vec4 card_color{0.10f, 0.12f, 0.16f, 1.0f};
        const glm::vec4 ruby{1.00f, 0.15f, 0.25f, 1.0f};
        const glm::vec4 emerald{0.15f, 1.00f, 0.35f, 1.0f};
        const glm::vec4 sapphire{0.20f, 0.45f, 1.00f, 1.0f};
        const glm::vec4 amber{1.00f, 0.80f, 0.15f, 1.0f};

        auto add_quad = [&](float x0, float y0, float x1, float y1, const glm::vec4& color) {
            uint16_t   base = static_cast<uint16_t>(vertices.size());
            TopoVertex v0{}, v1{}, v2{}, v3{};
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

            indices_u16.push_back(base + 0);
            indices_u16.push_back(base + 1);
            indices_u16.push_back(base + 2);
            indices_u16.push_back(base + 0);
            indices_u16.push_back(base + 2);
            indices_u16.push_back(base + 3);
        };

        const float hw = 0.36f;
        const float hh = 0.36f;

        // 1. Four base cards (indices 0..23, vertices 0..15 in ibuffer_u16)
        add_quad(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh, card_color);
        add_quad(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, card_color);
        add_quad(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, card_color);
        add_quad(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, card_color);

        auto add_diamond = [&](float cx, float cy, float rw, float rh, const glm::vec4& color) {
            TopoVertex v_top{}, v_left{}, v_bottom{}, v_right{};
            v_top.position    = {cx, cy + rh, 0.0f};
            v_top.color       = color;
            v_left.position   = {cx - rw, cy, 0.0f};
            v_left.color      = color;
            v_bottom.position = {cx, cy - rh, 0.0f};
            v_bottom.color    = color;
            v_right.position  = {cx + rw, cy, 0.0f};
            v_right.color     = color;

            vertices.push_back(v_top);
            vertices.push_back(v_left);
            vertices.push_back(v_bottom);
            vertices.push_back(v_right);
        };

        const float rw = 0.22f;
        const float rh = 0.22f;

        // 2. Q0 (Top-Left): Ruby Diamond (vertices 16..19)
        uint16_t q0_base = static_cast<uint16_t>(vertices.size());
        add_diamond(-0.5f, 0.5f, rw, rh, ruby);
        indices_u16.push_back(q0_base + 0);
        indices_u16.push_back(q0_base + 1);
        indices_u16.push_back(q0_base + 2);
        indices_u16.push_back(q0_base + 0);
        indices_u16.push_back(q0_base + 2);
        indices_u16.push_back(q0_base + 3);

        // 3. Q1 (Top-Right): Emerald Diamond (vertices 20..23)
        uint32_t q1_base = static_cast<uint32_t>(vertices.size());
        add_diamond(0.5f, 0.5f, rw, rh, emerald);
        // Put 6 dummy indices in ibuffer_u32 first (0..5)
        for (int i = 0; i < 6; ++i) {
            indices_u32.push_back(0);
        }
        // Then Q1 indices at first_index = 6 (6..11)
        indices_u32.push_back(q1_base + 0);
        indices_u32.push_back(q1_base + 1);
        indices_u32.push_back(q1_base + 2);
        indices_u32.push_back(q1_base + 0);
        indices_u32.push_back(q1_base + 2);
        indices_u32.push_back(q1_base + 3);

        // 4. Q2 (Bottom-Left): Sapphire Diamond (vertices 24..27)
        add_diamond(-0.5f, -0.5f, rw, rh, sapphire);
        // Indices in u16 buffer are local (0, 1, 2, 0, 2, 3) relative to base_vertex = 24!
        // At indices 30..35
        indices_u16.push_back(0);
        indices_u16.push_back(1);
        indices_u16.push_back(2);
        indices_u16.push_back(0);
        indices_u16.push_back(2);
        indices_u16.push_back(3);

        // 5. Q3 (Bottom-Right): Amber Diamond (vertices 28..31)
        add_diamond(0.5f, -0.5f, rw, rh, amber);
        // Pad 6 dummy indices in u16 buffer (indices 36..41)
        for (int i = 0; i < 6; ++i) {
            indices_u16.push_back(0);
        }
        // Indices at byte offset 42 * sizeof(uint16_t) are local relative to base_vertex = 28!
        // (indices 42..47)
        indices_u16.push_back(0);
        indices_u16.push_back(1);
        indices_u16.push_back(2);
        indices_u16.push_back(0);
        indices_u16.push_back(2);
        indices_u16.push_back(3);

        vbuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vertex_buffer";
            desc.size               = sizeof(TopoVertex) * vertices.size();
            desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        ibuffer_u16 = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "index_buffer_u16";
            desc.size               = sizeof(uint16_t) * indices_u16.size();
            desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        ibuffer_u32 = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "index_buffer_u32";
            desc.size               = sizeof(uint32_t) * indices_u32.size();
            desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        auto mapped_v = vbuffer.get_mapped_range<TopoVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(TopoVertex) * vertices.size());

        auto mapped_i16 = ibuffer_u16.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i16.data, indices_u16.data(), sizeof(uint16_t) * indices_u16.size());

        auto mapped_i32 = ibuffer_u32.get_mapped_range<uint32_t>();
        std::memcpy(mapped_i32.data, indices_u32.data(), sizeof(uint32_t) * indices_u32.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "index_formats_offsets_shader";
            desc.path   = "index_formats_offsets.slang";
            desc.source = index_formats_offsets_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(TopoVertex);
        pipeline.attributes.push_back({"position", offsetof(TopoVertex, position)});
        pipeline.attributes.push_back({"color", offsetof(TopoVertex, color)});

        GPUPrimitiveState prim{};
        prim.topology      = GPUPrimitiveTopology::TRIANGLE_LIST;
        prim.cull_mode     = GPUCullMode::NONE;
        pipeline.primitive = prim;

        pipeline.init_color_state(get_backbuffer_format());
        pipeline.init_vshader(device, module->get(), "vsmain");
        pipeline.init_fshader(device, module->get(), "fsmain");
        pipeline.init_playout(device, reflection->get());
        pipeline.init_pipeline(device, reflection->get());
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

        command.set_pipeline(pipeline.pipeline);
        command.set_vertex_buffer(0, vbuffer);

        // 1. Draw 4 base cards (indices 0..23 in ibuffer_u16)
        command.set_index_buffer(ibuffer_u16, GPUIndexFormat::UINT16, 0);
        command.draw_indexed(24, 1, 0, 0, 0);

        // 2. Q0 (Top-Left): UINT16 format, zero first_index (starts at 24), zero base_vertex
        command.draw_indexed(6, 1, 24, 0, 0);

        // 3. Q1 (Top-Right): UINT32 format, non-zero first_index = 6, zero base_vertex
        command.set_index_buffer(ibuffer_u32, GPUIndexFormat::UINT32, 0);
        command.draw_indexed(6, 1, 6, 0, 0);

        // 4. Q2 (Bottom-Left): UINT16 format, first_index = 30, non-zero base_vertex = 24
        command.set_index_buffer(ibuffer_u16, GPUIndexFormat::UINT16, 0);
        command.draw_indexed(6, 1, 30, 24, 0);

        // 5. Q3 (Bottom-Right): UINT16 format with byte offset = 42 * sizeof(uint16_t), base_vertex = 28
        command.set_index_buffer(ibuffer_u16, GPUIndexFormat::UINT16, 42 * sizeof(uint16_t));
        command.draw_indexed(6, 1, 0, 28, 0);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);

        command.submit();
    }
};

#if defined(LYRA_VULKAN_SUPPORT)
TEST_CASE("rhi::vulkan::pipeline_index_formats_offsets" * doctest::description("Testing index formats UINT16/UINT32 and base vertex/index offsets."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    IndexFormatsOffsetsApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::pipeline_index_formats_offsets" * doctest::description("Testing index formats UINT16/UINT32 and base vertex/index offsets."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    IndexFormatsOffsetsApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::pipeline_index_formats_offsets" * doctest::description("Testing index formats UINT16/UINT32 and base vertex/index offsets."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    IndexFormatsOffsetsApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::pipeline_index_formats_offsets" * doctest::description("Testing index formats UINT16/UINT32 and base vertex/index offsets."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    IndexFormatsOffsetsApp(desc).run();
}
