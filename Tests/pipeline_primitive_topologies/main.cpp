#include "helper.h"

CString primitive_topologies_program = R"""(
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

struct PrimitiveTopologiesApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    SimpleRenderPipeline pipeline_tri_list;
    SimpleRenderPipeline pipeline_tri_strip;

    explicit PrimitiveTopologiesApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<TopoVertex> vertices;
        Vector<uint32_t>   indices;

        const glm::vec4 card_color{0.10f, 0.12f, 0.16f, 1.0f};
        const glm::vec4 ruby{1.00f, 0.15f, 0.25f, 1.0f};
        const glm::vec4 emerald{0.15f, 1.00f, 0.35f, 1.0f};
        const glm::vec4 sapphire{0.20f, 0.45f, 1.00f, 1.0f};
        const glm::vec4 amber{1.00f, 0.80f, 0.15f, 1.0f};

        auto add_quad = [&](float x0, float y0, float x1, float y1, const glm::vec4& color) {
            uint32_t   base = static_cast<uint32_t>(vertices.size());
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

            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        const float hw = 0.36f;
        const float hh = 0.36f;

        // 1. Four base cards (indices 0..23, vertices 0..15, drawn with pipeline_tri_list)
        add_quad(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh, card_color);
        add_quad(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, card_color);
        add_quad(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, card_color);
        add_quad(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, card_color);

        // 2. Q0 (Top-Left): Indexed TRIANGLE_LIST Diamond (indices 24..29, vertices 16..19)
        uint32_t    q0_base = static_cast<uint32_t>(vertices.size());
        const float rw      = 0.22f;
        const float rh      = 0.22f;
        TopoVertex  q0_top{}, q0_left{}, q0_bottom{}, q0_right{};
        q0_top.position    = {-0.5f, 0.5f + rh, 0.0f};
        q0_top.color       = ruby;
        q0_left.position   = {-0.5f - rw, 0.5f, 0.0f};
        q0_left.color      = ruby;
        q0_bottom.position = {-0.5f, 0.5f - rh, 0.0f};
        q0_bottom.color    = ruby;
        q0_right.position  = {-0.5f + rw, 0.5f, 0.0f};
        q0_right.color     = ruby;
        vertices.push_back(q0_top);
        vertices.push_back(q0_left);
        vertices.push_back(q0_bottom);
        vertices.push_back(q0_right);

        indices.push_back(q0_base + 0);
        indices.push_back(q0_base + 1);
        indices.push_back(q0_base + 2);
        indices.push_back(q0_base + 0);
        indices.push_back(q0_base + 2);
        indices.push_back(q0_base + 3);

        // 3. Q1 (Top-Right): Non-indexed TRIANGLE_STRIP (4 vertices: 20..23)
        // Diamond formed by 4 strip vertices
        TopoVertex q1_0{}, q1_1{}, q1_2{}, q1_3{};
        q1_0.position = {0.5f - rw, 0.5f, 0.0f}; // left
        q1_0.color    = emerald;
        q1_1.position = {0.5f, 0.5f - rh, 0.0f}; // bottom
        q1_1.color    = emerald;
        q1_2.position = {0.5f, 0.5f + rh, 0.0f}; // top
        q1_2.color    = emerald;
        q1_3.position = {0.5f + rw, 0.5f, 0.0f}; // right
        q1_3.color    = emerald;
        vertices.push_back(q1_0);
        vertices.push_back(q1_1);
        vertices.push_back(q1_2);
        vertices.push_back(q1_3);

        // 4. Q2 (Bottom-Left): Indexed TRIANGLE_STRIP (6 vertices: 24..29, indices 30..35)
        // Faceted badge formed by 6 strip vertices (4 triangles)
        uint32_t   q2_base = static_cast<uint32_t>(vertices.size());
        TopoVertex q2_0{}, q2_1{}, q2_2{}, q2_3{}, q2_4{}, q2_5{};
        q2_0.position = {-0.5f - 0.20f, -0.5f + 0.12f, 0.0f};
        q2_0.color    = sapphire;
        q2_1.position = {-0.5f - 0.20f, -0.5f - 0.12f, 0.0f};
        q2_1.color    = sapphire;
        q2_2.position = {-0.5f, -0.5f + 0.22f, 0.0f};
        q2_2.color    = sapphire;
        q2_3.position = {-0.5f, -0.5f - 0.22f, 0.0f};
        q2_3.color    = sapphire;
        q2_4.position = {-0.5f + 0.20f, -0.5f + 0.12f, 0.0f};
        q2_4.color    = sapphire;
        q2_5.position = {-0.5f + 0.20f, -0.5f - 0.12f, 0.0f};
        q2_5.color    = sapphire;
        vertices.push_back(q2_0);
        vertices.push_back(q2_1);
        vertices.push_back(q2_2);
        vertices.push_back(q2_3);
        vertices.push_back(q2_4);
        vertices.push_back(q2_5);

        for (uint32_t i = 0; i < 6; ++i) {
            indices.push_back(q2_base + i);
        }

        // 5. Q3 (Bottom-Right): Non-indexed TRIANGLE_STRIP (6 vertices: 30..35)
        // Chevron ribbon badge formed by 6 strip vertices (4 triangles)
        TopoVertex q3_0{}, q3_1{}, q3_2{}, q3_3{}, q3_4{}, q3_5{};
        q3_0.position = {0.5f - 0.22f, -0.5f - 0.12f, 0.0f};
        q3_0.color    = amber;
        q3_1.position = {0.5f - 0.16f, -0.5f - 0.22f, 0.0f};
        q3_1.color    = amber;
        q3_2.position = {0.5f, -0.5f + 0.15f, 0.0f};
        q3_2.color    = amber;
        q3_3.position = {0.5f, -0.5f + 0.02f, 0.0f};
        q3_3.color    = amber;
        q3_4.position = {0.5f + 0.22f, -0.5f - 0.12f, 0.0f};
        q3_4.color    = amber;
        q3_5.position = {0.5f + 0.16f, -0.5f - 0.22f, 0.0f};
        q3_5.color    = amber;
        vertices.push_back(q3_0);
        vertices.push_back(q3_1);
        vertices.push_back(q3_2);
        vertices.push_back(q3_3);
        vertices.push_back(q3_4);
        vertices.push_back(q3_5);

        vbuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vertex_buffer";
            desc.size               = sizeof(TopoVertex) * vertices.size();
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

        auto mapped_v = vbuffer.get_mapped_range<TopoVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(TopoVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint32_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint32_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "primitive_topologies_shader";
            desc.path   = "primitive_topologies.slang";
            desc.source = primitive_topologies_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        auto init_pipe = [&](SimpleRenderPipeline& pipe, GPUPrimitiveTopology topology) {
            pipe.vstride = sizeof(TopoVertex);
            pipe.attributes.push_back({"position", offsetof(TopoVertex, position)});
            pipe.attributes.push_back({"color", offsetof(TopoVertex, color)});

            GPUPrimitiveState prim{};
            prim.topology  = topology;
            prim.cull_mode = GPUCullMode::NONE;
            pipe.primitive = prim;

            pipe.init_color_state(get_backbuffer_format());
            pipe.init_vshader(device, module.get(), "vsmain");
            pipe.init_fshader(device, module.get(), "fsmain");
            pipe.init_playout(device, reflection.get());
            pipe.init_pipeline(device, reflection.get());
        };

        init_pipe(pipeline_tri_list, GPUPrimitiveTopology::TRIANGLE_LIST);
        init_pipe(pipeline_tri_strip, GPUPrimitiveTopology::TRIANGLE_STRIP);
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

        // 1. Draw base cards (24 indices) with pipeline_tri_list
        command.set_pipeline(pipeline_tri_list.pipeline);
        command.draw_indexed(24, 1, 0, 0, 0);

        // 2. Q0 Top-Left: Indexed TRIANGLE_LIST diamond (6 indices, offset 24)
        command.draw_indexed(6, 1, 24, 0, 0);

        // 3. Q1 Top-Right: Non-indexed TRIANGLE_STRIP diamond (4 vertices, start 20)
        command.set_pipeline(pipeline_tri_strip.pipeline);
        command.draw(4, 1, 20, 0);

        // 4. Q2 Bottom-Left: Indexed TRIANGLE_STRIP faceted badge (6 indices, offset 30)
        command.draw_indexed(6, 1, 30, 0, 0);

        // 5. Q3 Bottom-Right: Non-indexed TRIANGLE_STRIP chevron badge (6 vertices, start 30)
        command.draw(6, 1, 30, 0);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::pipeline_primitive_topologies" * doctest::description("Testing rasterizer primitive assembly topologies."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PrimitiveTopologiesApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::pipeline_primitive_topologies" * doctest::description("Testing rasterizer primitive assembly topologies."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PrimitiveTopologiesApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::pipeline_primitive_topologies" * doctest::description("Testing rasterizer primitive assembly topologies."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PrimitiveTopologiesApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::pipeline_primitive_topologies" * doctest::description("Testing rasterizer primitive assembly topologies."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    PrimitiveTopologiesApp(desc).run();
}
