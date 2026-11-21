#include "helper.h"

/**
 * @file main.cpp
 * @brief RHI validation test for instanced drawing operations.
 *
 * Exercises GPUVertexStepMode::VERTEX and GPUVertexStepMode::INSTANCE across:
 * - Quadrant 0 (Top-Left): Non-indexed instanced draw (draw) with first_instance = 0.
 * - Quadrant 1 (Top-Right): Indexed instanced draw (draw_indexed) with base_vertex and first_instance.
 * - Quadrant 2 (Bottom-Left): Non-indexed instanced draw with non-zero first_instance and first_vertex.
 * - Quadrant 3 (Bottom-Right): Indexed instanced draw combining first_index, base_vertex, and first_instance.
 */

using namespace lyra;

CString instanced_shader_program = R"""(
struct VertexInput
{
    float2 position      : POSITION;
    float2 inst_position : INSTPOS;
    float4 inst_color    : INSTCOLOR;
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
    output.position = float4(input.position + input.inst_position, 0.0, 1.0);
    output.color = input.inst_color;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target0
{
    return input.color;
}
)""";

CString card_shader_program = R"""(
struct VertexInput
{
    float2 position : POSITION;
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
    output.position = float4(input.position, 0.0, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target0
{
    return input.color;
}
)""";

struct BaseVertex
{
    glm::vec2 position;
};

struct InstanceData
{
    glm::vec2 position;
    glm::vec4 color;
};

struct CardVertex
{
    glm::vec2 position;
    glm::vec4 color;
};

struct InstancedDrawingApp : public TestApp
{
    GPUBuffer            card_vbuffer;
    SimpleRenderPipeline card_pipeline;

    GPUBuffer            base_vbuffer;
    GPUBuffer            instance_buffer;
    GPUBuffer            index_buffer;
    GPURenderPipeline    instanced_pipeline;

    explicit InstancedDrawingApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_cards();
        setup_instanced_geometry();
    }

    void setup_cards()
    {
        auto& device = RHI::get_current_device();

        Vector<CardVertex> card_vertices;
        const glm::vec4    card_color{0.082f, 0.094f, 0.133f, 1.0f};

        auto add_card = [&](float x0, float y0, float x1, float y1) {
            CardVertex v0{{x0, y0}, card_color};
            CardVertex v1{{x1, y0}, card_color};
            CardVertex v2{{x1, y1}, card_color};
            CardVertex v3{{x0, y1}, card_color};

            card_vertices.push_back(v0);
            card_vertices.push_back(v1);
            card_vertices.push_back(v2);
            card_vertices.push_back(v0);
            card_vertices.push_back(v2);
            card_vertices.push_back(v3);
        };

        // 4 quadrant background cards
        add_card(-0.90f, 0.10f, -0.10f, 0.90f); // Top-Left
        add_card(0.10f, 0.10f, 0.90f, 0.90f);   // Top-Right
        add_card(-0.90f, -0.90f, -0.10f, -0.10f); // Bottom-Left
        add_card(0.10f, -0.90f, 0.90f, -0.10f);   // Bottom-Right

        auto bdesc        = GPUBufferDescriptor{};
        bdesc.size        = sizeof(CardVertex) * card_vertices.size();
        bdesc.usage       = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        bdesc.mapped_at_creation = true;
        card_vbuffer      = device.create_buffer(bdesc);

        auto mapped = card_vbuffer.get_mapped_range<CardVertex>();
        std::memcpy(mapped.data, card_vertices.data(), bdesc.size);

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "instanced_cards_shader";
            desc.path   = "instanced_cards.slang";
            desc.source = card_shader_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        card_pipeline.vstride = sizeof(CardVertex);
        card_pipeline.attributes.push_back({"position", offsetof(CardVertex, position)});
        card_pipeline.attributes.push_back({"color", offsetof(CardVertex, color)});

        GPUPrimitiveState prim{};
        prim.topology            = GPUPrimitiveTopology::TRIANGLE_LIST;
        prim.cull_mode           = GPUCullMode::NONE;
        card_pipeline.primitive  = prim;

        card_pipeline.init_color_state(get_backbuffer_format());
        card_pipeline.init_vshader(device, module.get(), "vsmain");
        card_pipeline.init_fshader(device, module.get(), "fsmain");
        card_pipeline.init_playout(device, reflection.get());
        card_pipeline.init_pipeline(device, reflection.get());
    }

    void setup_instanced_geometry()
    {
        auto& device = RHI::get_current_device();

        Vector<BaseVertex>   base_vertices;
        Vector<uint16_t>     indices;
        Vector<InstanceData> instances;

        // --- Mesh 0: Diamond (indices 0..5, 6 vertices) for Quadrant 0 non-indexed draw
        // Base centered at (0, 0)
        base_vertices.push_back({{-0.07f, 0.00f}});
        base_vertices.push_back({{0.00f, 0.09f}});
        base_vertices.push_back({{0.07f, 0.00f}});

        base_vertices.push_back({{-0.07f, 0.00f}});
        base_vertices.push_back({{0.07f, 0.00f}});
        base_vertices.push_back({{0.00f, -0.09f}});

        // --- Mesh 1: 4-pointed Star (base vertex 6, 9 vertices, 24 indices) for Quadrant 1 indexed draw
        // Vertices 6..14:
        // Center vertex
        base_vertices.push_back({{0.00f, 0.00f}}); // vertex 6 (local 0)
        // 8 points: alternating outer (0.08) and inner (0.035)
        for (int i = 0; i < 8; ++i) {
            float angle = static_cast<float>(i) * (3.14159265f / 4.0f);
            float r     = (i % 2 == 0) ? 0.08f : 0.035f;
            base_vertices.push_back({{std::cos(angle) * r, std::sin(angle) * r}}); // local 1..8
        }
        for (int i = 0; i < 8; ++i) {
            indices.push_back(0); // center
            indices.push_back(static_cast<uint16_t>(1 + i));
            indices.push_back(static_cast<uint16_t>(1 + ((i + 1) % 8)));
        }

        // --- Mesh 2: Arrow Pennant (base vertex 15, 3 vertices) for Quadrant 2 non-indexed draw
        base_vertices.push_back({{-0.06f, -0.07f}}); // 15
        base_vertices.push_back({{0.00f, 0.08f}});   // 16
        base_vertices.push_back({{0.06f, -0.07f}});  // 17

        // --- Mesh 3: Hexagon (base vertex 18, 7 vertices, 18 indices) for Quadrant 3 indexed draw
        base_vertices.push_back({{0.00f, 0.00f}}); // vertex 18 (local 0)
        for (int i = 0; i < 6; ++i) {
            float angle = static_cast<float>(i) * (3.14159265f / 3.0f);
            float r     = 0.075f;
            base_vertices.push_back({{std::cos(angle) * r, std::sin(angle) * r}}); // local 1..6
        }
        for (int i = 0; i < 6; ++i) {
            indices.push_back(0); // center
            indices.push_back(static_cast<uint16_t>(1 + i));
            indices.push_back(static_cast<uint16_t>(1 + ((i + 1) % 6)));
        }

        // --- Instance Data:
        // Quadrant 0: instances 0..3 (2x2 cluster in Top-Left, center (-0.5, 0.5))
        instances.push_back({{-0.65f, 0.65f}, {1.00f, 0.32f, 0.32f, 1.0f}}); // Coral
        instances.push_back({{-0.35f, 0.65f}, {0.00f, 0.90f, 0.46f, 1.0f}}); // Mint
        instances.push_back({{-0.65f, 0.35f}, {1.00f, 0.92f, 0.00f, 1.0f}}); // Gold
        instances.push_back({{-0.35f, 0.35f}, {0.00f, 0.90f, 1.00f, 1.0f}}); // Cyan

        // Quadrant 1: instances 4..7 (Diamond constellation in Top-Right, center (0.5, 0.5))
        instances.push_back({{0.50f, 0.70f}, {0.83f, 0.00f, 0.98f, 1.0f}}); // Purple
        instances.push_back({{0.32f, 0.50f}, {0.16f, 0.47f, 1.00f, 1.0f}}); // Electric Blue
        instances.push_back({{0.68f, 0.50f}, {1.00f, 0.43f, 0.00f, 1.0f}}); // Neon Orange
        instances.push_back({{0.50f, 0.30f}, {0.00f, 0.78f, 0.33f, 1.0f}}); // Emerald

        // Quadrant 2: instances 8..11 (2x2 cluster in Bottom-Left, center (-0.5, -0.5))
        instances.push_back({{-0.65f, -0.35f}, {1.00f, 0.25f, 0.51f, 1.0f}}); // Pink
        instances.push_back({{-0.35f, -0.35f}, {0.68f, 0.92f, 0.00f, 1.0f}}); // Lime
        instances.push_back({{-0.65f, -0.65f}, {1.00f, 0.84f, 0.00f, 1.0f}}); // Amber
        instances.push_back({{-0.35f, -0.65f}, {0.40f, 0.12f, 1.00f, 1.0f}}); // Violet

        // Quadrant 3: instances 12..14 (Triad in Bottom-Right, center (0.5, -0.5))
        instances.push_back({{0.50f, -0.36f}, {0.11f, 0.91f, 0.71f, 1.0f}}); // Turquoise
        instances.push_back({{0.35f, -0.62f}, {1.00f, 1.00f, 0.00f, 1.0f}}); // Bright Yellow
        instances.push_back({{0.65f, -0.62f}, {0.96f, 0.00f, 0.34f, 1.0f}}); // Ruby

        // Create GPU Buffers
        auto bdesc_v         = GPUBufferDescriptor{};
        bdesc_v.size         = sizeof(BaseVertex) * base_vertices.size();
        bdesc_v.usage        = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        bdesc_v.mapped_at_creation = true;
        base_vbuffer         = device.create_buffer(bdesc_v);
        auto mapped_v        = base_vbuffer.get_mapped_range<BaseVertex>();
        std::memcpy(mapped_v.data, base_vertices.data(), bdesc_v.size);

        auto bdesc_i         = GPUBufferDescriptor{};
        bdesc_i.size         = sizeof(uint16_t) * indices.size();
        bdesc_i.usage        = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        bdesc_i.mapped_at_creation = true;
        index_buffer         = device.create_buffer(bdesc_i);
        auto mapped_i        = index_buffer.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i.data, indices.data(), bdesc_i.size);

        auto bdesc_inst      = GPUBufferDescriptor{};
        bdesc_inst.size      = sizeof(InstanceData) * instances.size();
        bdesc_inst.usage     = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        bdesc_inst.mapped_at_creation = true;
        instance_buffer      = device.create_buffer(bdesc_inst);
        auto mapped_inst     = instance_buffer.get_mapped_range<InstanceData>();
        std::memcpy(mapped_inst.data, instances.data(), bdesc_inst.size);

        // Compile instanced shader
        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "instanced_drawing_shader";
            desc.path   = "instanced_drawing.slang";
            desc.source = instanced_shader_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        SimplePipeline instanced_base;
        instanced_base.init_color_state(get_backbuffer_format());
        instanced_base.init_vshader(device, module.get(), "vsmain");
        instanced_base.init_fshader(device, module.get(), "fsmain");
        instanced_base.init_playout(device, reflection.get());

        // Multi-buffer layouts:
        // Slot 0: Per-vertex base geometry
        Array<GPUVertexAttribute, 1> slot0_attrs{};
        slot0_attrs[0] = {GPUVertexFormat::FLOAT32x2, 0, 0, "position"};

        // Slot 1: Per-instance position offset and color
        Array<GPUVertexAttribute, 2> slot1_attrs{};
        slot1_attrs[0] = {GPUVertexFormat::FLOAT32x2, offsetof(InstanceData, position), 1, "inst_position"};
        slot1_attrs[1] = {GPUVertexFormat::FLOAT32x4, offsetof(InstanceData, color), 2, "inst_color"};

        Array<GPUVertexBufferLayout, 2> buffer_layouts{};
        buffer_layouts[0].array_stride = sizeof(BaseVertex);
        buffer_layouts[0].step_mode    = GPUVertexStepMode::VERTEX;
        buffer_layouts[0].attributes   = slot0_attrs;

        buffer_layouts[1].array_stride = sizeof(InstanceData);
        buffer_layouts[1].step_mode    = GPUVertexStepMode::INSTANCE;
        buffer_layouts[1].attributes   = slot1_attrs;

        GPURenderPipelineDescriptor pdesc{};
        pdesc.layout               = instanced_base.playout;
        pdesc.primitive.topology   = GPUPrimitiveTopology::TRIANGLE_LIST;
        pdesc.primitive.cull_mode  = GPUCullMode::NONE;
        pdesc.vertex.module        = instanced_base.vshader;
        pdesc.vertex.entry_point   = "vsmain";
        pdesc.vertex.buffers       = buffer_layouts;
        pdesc.fragment.module      = instanced_base.fshader;
        pdesc.fragment.entry_point = "fsmain";
        pdesc.fragment.targets     = instanced_base.rstates;

        instanced_pipeline = device.create_render_pipeline(pdesc);
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
        color_attachment.clear_value = GPUColor{0.055f, 0.063f, 0.090f, 1.0f};
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

        // 1. Draw quadrant cards
        command.set_pipeline(card_pipeline.pipeline);
        command.set_vertex_buffer(0, card_vbuffer);
        command.draw(24, 1, 0, 0);

        // 2. Bind instanced pipeline and buffers
        command.set_pipeline(instanced_pipeline);
        command.set_vertex_buffer(0, base_vbuffer);
        command.set_vertex_buffer(1, instance_buffer);
        command.set_index_buffer(index_buffer, GPUIndexFormat::UINT16, 0);

        // Quadrant 0 (Top-Left): Non-indexed instanced draw (first_vertex = 0, first_instance = 0, count = 4)
        command.draw(6, 4, 0, 0);

        // Quadrant 1 (Top-Right): Indexed instanced draw (index_count = 24, first_index = 0, base_vertex = 6, first_instance = 4, count = 4)
        command.draw_indexed(24, 4, 0, 6, 4);

        // Quadrant 2 (Bottom-Left): Non-indexed instanced draw with non-zero first_instance and first_vertex (first_vertex = 15, first_instance = 8, count = 4)
        command.draw(3, 4, 15, 8);

        // Quadrant 3 (Bottom-Right): Indexed instanced draw with first_index, base_vertex, and first_instance (index_count = 18, first_index = 24, base_vertex = 18, first_instance = 12, count = 3)
        command.draw_indexed(18, 3, 24, 18, 12);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);

        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::instanced_drawing" * doctest::description("Testing instanced drawing with vertex and instance step modes."))
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
    InstancedDrawingApp(desc).run();
}
#endif

#if defined(_WIN32)
TEST_CASE("rhi::d3d12::instanced_drawing" * doctest::description("Testing instanced drawing with vertex and instance step modes."))
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
    InstancedDrawingApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::instanced_drawing" * doctest::description("Testing instanced drawing with vertex and instance step modes."))
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
    InstancedDrawingApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::instanced_drawing" * doctest::description("Testing instanced drawing with vertex and instance step modes."))
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
    InstancedDrawingApp(desc).run();
}
