#include "helper.h"

/**
 * @file main.cpp
 * @brief RHI validation test for indirect drawing commands (draw_indirect and draw_indexed_indirect).
 *
 * Exercises buffer-driven indirect execution across four isolated quadrants:
 * - Quadrant 0 (Top-Left): Non-indexed indirect draw (draw_indirect) with draw_count = 1 at offset 0.
 * - Quadrant 1 (Top-Right): Indexed indirect draw (draw_indexed_indirect) with draw_count = 1 and base_vertex.
 * - Quadrant 2 (Bottom-Left): Multi-draw indirect (draw_indirect) with draw_count = 2 and non-zero indirect_offset.
 * - Quadrant 3 (Bottom-Right): Indexed indirect draw (draw_indexed_indirect) with non-zero indirect_offset,
 *   first_index, and base_vertex.
 */

using namespace lyra;

CString indirect_shader_program = R"""(
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
float4 fsmain(VertexOutput input) : SV_Target0
{
    return input.color;
}
)""";

struct BadgeVertex
{
    glm::vec3 position;
    float     _pad = 0.0f;
    glm::vec4 color;
};

struct DrawIndirectArgs
{
    uint32_t vertex_count;
    uint32_t instance_count;
    uint32_t first_vertex;
    uint32_t first_instance;
};

struct DrawIndexedIndirectArgs
{
    uint32_t index_count;
    uint32_t instance_count;
    uint32_t first_index;
    int32_t  base_vertex;
    uint32_t first_instance;
};

struct IndirectDrawingApp : public TestApp
{
    GPUBuffer            card_vbuffer;
    SimpleRenderPipeline card_pipeline;

    GPUBuffer            geometry_vbuffer;
    GPUBuffer            geometry_ibuffer;
    GPUBuffer            indirect_vbuffer;
    GPUBuffer            indirect_ibuffer;
    SimpleRenderPipeline badge_pipeline;

    explicit IndirectDrawingApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_cards();
        setup_geometry_and_indirect_buffers();
    }

    void setup_cards()
    {
        auto& device = RHI::get_current_device();

        Vector<BadgeVertex> card_vertices;
        const glm::vec4     card_color{0.082f, 0.094f, 0.133f, 1.0f};

        auto add_card = [&](float x0, float y0, float x1, float y1) {
            BadgeVertex v0{{x0, y0, 0.0f}, 0.0f, card_color};
            BadgeVertex v1{{x1, y0, 0.0f}, 0.0f, card_color};
            BadgeVertex v2{{x1, y1, 0.0f}, 0.0f, card_color};
            BadgeVertex v3{{x0, y1, 0.0f}, 0.0f, card_color};

            card_vertices.push_back(v0);
            card_vertices.push_back(v1);
            card_vertices.push_back(v2);
            card_vertices.push_back(v0);
            card_vertices.push_back(v2);
            card_vertices.push_back(v3);
        };

        // 4 quadrant background cards
        add_card(-0.90f, 0.10f, -0.10f, 0.90f);   // Top-Left
        add_card(0.10f, 0.10f, 0.90f, 0.90f);     // Top-Right
        add_card(-0.90f, -0.90f, -0.10f, -0.10f); // Bottom-Left
        add_card(0.10f, -0.90f, 0.90f, -0.10f);   // Bottom-Right

        auto bdesc               = GPUBufferDescriptor{};
        bdesc.size               = sizeof(BadgeVertex) * card_vertices.size();
        bdesc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        bdesc.mapped_at_creation = true;
        card_vbuffer             = device.create_buffer(bdesc);

        auto mapped = card_vbuffer.get_mapped_range<BadgeVertex>();
        std::memcpy(mapped.data, card_vertices.data(), bdesc.size);

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "indirect_cards_shader";
            desc.path   = "indirect_cards.slang";
            desc.source = indirect_shader_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        card_pipeline.vstride = sizeof(BadgeVertex);
        card_pipeline.attributes.push_back({"position", offsetof(BadgeVertex, position)});
        card_pipeline.attributes.push_back({"color", offsetof(BadgeVertex, color)});

        GPUPrimitiveState prim{};
        prim.topology           = GPUPrimitiveTopology::TRIANGLE_LIST;
        prim.cull_mode          = GPUCullMode::NONE;
        card_pipeline.primitive = prim;

        card_pipeline.init_color_state(get_backbuffer_format());
        card_pipeline.init_vshader(device, module.get(), "vsmain");
        card_pipeline.init_fshader(device, module.get(), "fsmain");
        card_pipeline.init_playout(device, reflection.get());
        card_pipeline.init_pipeline(device, reflection.get());
    }

    void setup_geometry_and_indirect_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<BadgeVertex>             vertices;
        Vector<uint16_t>                indices;
        Vector<DrawIndirectArgs>        draw_cmds;
        Vector<DrawIndexedIndirectArgs> draw_indexed_cmds;

        // Palette
        const glm::vec4 emerald{0.00f, 0.90f, 0.46f, 1.0f};  // Quadrant 0
        const glm::vec4 sapphire{0.16f, 0.47f, 1.00f, 1.0f}; // Quadrant 1
        const glm::vec4 amber{1.00f, 0.84f, 0.00f, 1.0f};    // Quadrant 2 Upper
        const glm::vec4 coral{1.00f, 0.34f, 0.13f, 1.0f};    // Quadrant 2 Lower
        const glm::vec4 ruby{0.96f, 0.00f, 0.34f, 1.0f};     // Quadrant 3

        // --- Mesh 0 (Vertices 0..5, 6 vertices): Diamond in Quadrant 0 (Top-Left, center (-0.50, 0.50))
        float cx0 = -0.50f, cy0 = 0.50f, r0 = 0.20f;
        vertices.push_back({{cx0 - r0, cy0, 0.0f}, 0.0f, emerald});
        vertices.push_back({{cx0, cy0 + r0, 0.0f}, 0.0f, emerald});
        vertices.push_back({{cx0 + r0, cy0, 0.0f}, 0.0f, emerald});

        vertices.push_back({{cx0 - r0, cy0, 0.0f}, 0.0f, emerald});
        vertices.push_back({{cx0 + r0, cy0, 0.0f}, 0.0f, emerald});
        vertices.push_back({{cx0, cy0 - r0, 0.0f}, 0.0f, emerald});

        // --- Mesh 1 (Vertices 6..14, 9 vertices, 24 indices): 4-Pointed Star in Quadrant 1 (Top-Right, center (0.50, 0.50))
        float cx1 = 0.50f, cy1 = 0.50f;
        vertices.push_back({{cx1, cy1, 0.0f}, 0.0f, sapphire}); // local 0 (index 6)
        for (int i = 0; i < 8; ++i) {
            float angle = static_cast<float>(i) * (3.14159265f / 4.0f);
            float r     = (i % 2 == 0) ? 0.22f : 0.09f;
            vertices.push_back({{cx1 + std::cos(angle) * r, cy1 + std::sin(angle) * r, 0.0f}, 0.0f, sapphire});
        }
        for (int i = 0; i < 8; ++i) {
            indices.push_back(0); // center (relative to base_vertex = 6)
            indices.push_back(static_cast<uint16_t>(1 + i));
            indices.push_back(static_cast<uint16_t>(1 + ((i + 1) % 8)));
        }

        // --- Mesh 2 (Vertices 15..20, 6 vertices): Upper Chevron in Quadrant 2 (Bottom-Left, center (-0.50, -0.36))
        float cx2 = -0.50f, cy2a = -0.36f;
        vertices.push_back({{cx2 - 0.22f, cy2a - 0.08f, 0.0f}, 0.0f, amber});
        vertices.push_back({{cx2, cy2a + 0.10f, 0.0f}, 0.0f, amber});
        vertices.push_back({{cx2 + 0.22f, cy2a - 0.08f, 0.0f}, 0.0f, amber});

        vertices.push_back({{cx2 - 0.22f, cy2a - 0.08f, 0.0f}, 0.0f, amber});
        vertices.push_back({{cx2 + 0.22f, cy2a - 0.08f, 0.0f}, 0.0f, amber});
        vertices.push_back({{cx2, cy2a + 0.02f, 0.0f}, 0.0f, amber});

        // --- Mesh 3 (Vertices 21..26, 6 vertices): Lower Chevron in Quadrant 2 (Bottom-Left, center (-0.50, -0.64))
        float cy2b = -0.64f;
        vertices.push_back({{cx2 - 0.22f, cy2b - 0.08f, 0.0f}, 0.0f, coral});
        vertices.push_back({{cx2, cy2b + 0.10f, 0.0f}, 0.0f, coral});
        vertices.push_back({{cx2 + 0.22f, cy2b - 0.08f, 0.0f}, 0.0f, coral});

        vertices.push_back({{cx2 - 0.22f, cy2b - 0.08f, 0.0f}, 0.0f, coral});
        vertices.push_back({{cx2 + 0.22f, cy2b - 0.08f, 0.0f}, 0.0f, coral});
        vertices.push_back({{cx2, cy2b + 0.02f, 0.0f}, 0.0f, coral});

        // --- Mesh 4 (Vertices 27..33, 7 vertices, 18 indices): Hexagon in Quadrant 3 (Bottom-Right, center (0.50, -0.50))
        float cx3 = 0.50f, cy3 = -0.50f;
        vertices.push_back({{cx3, cy3, 0.0f}, 0.0f, ruby}); // local 0 (vertex 27)
        for (int i = 0; i < 6; ++i) {
            float angle = static_cast<float>(i) * (3.14159265f / 3.0f);
            float r     = 0.18f;
            vertices.push_back({{cx3 + std::cos(angle) * r, cy3 + std::sin(angle) * r, 0.0f}, 0.0f, ruby});
        }
        for (int i = 0; i < 6; ++i) {
            indices.push_back(0); // center (relative to base_vertex = 27)
            indices.push_back(static_cast<uint16_t>(1 + i));
            indices.push_back(static_cast<uint16_t>(1 + ((i + 1) % 6)));
        }

        // --- Indirect Draw Commands (Non-indexed):
        // Command 0 (Offset 0): Quadrant 0 Diamond
        draw_cmds.push_back({6, 1, 0, 0});
        // Command 1 (Offset 16): Quadrant 2 Upper Chevron
        draw_cmds.push_back({6, 1, 15, 0});
        // Command 2 (Offset 32): Quadrant 2 Lower Chevron
        draw_cmds.push_back({6, 1, 21, 0});

        // --- Indirect Draw Commands (Indexed):
        // Command 0 (Offset 0): Quadrant 1 Star (24 indices, first_index = 0, base_vertex = 6)
        draw_indexed_cmds.push_back({24, 1, 0, 6, 0});
        // Command 1 (Offset 20 = sizeof(DrawIndexedIndirectArgs)): Quadrant 3 Hexagon (18 indices, first_index = 24, base_vertex = 27)
        draw_indexed_cmds.push_back({18, 1, 24, 27, 0});

        // Create GPU Buffers
        auto bdesc_v               = GPUBufferDescriptor{};
        bdesc_v.size               = sizeof(BadgeVertex) * vertices.size();
        bdesc_v.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        bdesc_v.mapped_at_creation = true;
        geometry_vbuffer           = device.create_buffer(bdesc_v);
        auto mapped_v              = geometry_vbuffer.get_mapped_range<BadgeVertex>();
        std::memcpy(mapped_v.data, vertices.data(), bdesc_v.size);

        auto bdesc_i               = GPUBufferDescriptor{};
        bdesc_i.size               = sizeof(uint16_t) * indices.size();
        bdesc_i.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        bdesc_i.mapped_at_creation = true;
        geometry_ibuffer           = device.create_buffer(bdesc_i);
        auto mapped_i              = geometry_ibuffer.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i.data, indices.data(), bdesc_i.size);

        auto bdesc_iv               = GPUBufferDescriptor{};
        bdesc_iv.size               = sizeof(DrawIndirectArgs) * draw_cmds.size();
        bdesc_iv.usage              = GPUBufferUsage::INDIRECT | GPUBufferUsage::MAP_WRITE;
        bdesc_iv.mapped_at_creation = true;
        indirect_vbuffer            = device.create_buffer(bdesc_iv);
        auto mapped_iv              = indirect_vbuffer.get_mapped_range<DrawIndirectArgs>();
        std::memcpy(mapped_iv.data, draw_cmds.data(), bdesc_iv.size);

        auto bdesc_ii               = GPUBufferDescriptor{};
        bdesc_ii.size               = sizeof(DrawIndexedIndirectArgs) * draw_indexed_cmds.size();
        bdesc_ii.usage              = GPUBufferUsage::INDIRECT | GPUBufferUsage::MAP_WRITE;
        bdesc_ii.mapped_at_creation = true;
        indirect_ibuffer            = device.create_buffer(bdesc_ii);
        auto mapped_ii              = indirect_ibuffer.get_mapped_range<DrawIndexedIndirectArgs>();
        std::memcpy(mapped_ii.data, draw_indexed_cmds.data(), bdesc_ii.size);

        // Compile Badge Shader Pipeline
        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "indirect_badges_shader";
            desc.path   = "indirect_badges.slang";
            desc.source = indirect_shader_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        badge_pipeline.vstride = sizeof(BadgeVertex);
        badge_pipeline.attributes.push_back({"position", offsetof(BadgeVertex, position)});
        badge_pipeline.attributes.push_back({"color", offsetof(BadgeVertex, color)});

        GPUPrimitiveState prim{};
        prim.topology            = GPUPrimitiveTopology::TRIANGLE_LIST;
        prim.cull_mode           = GPUCullMode::NONE;
        badge_pipeline.primitive = prim;

        badge_pipeline.init_color_state(get_backbuffer_format());
        badge_pipeline.init_vshader(device, module.get(), "vsmain");
        badge_pipeline.init_fshader(device, module.get(), "fsmain");
        badge_pipeline.init_playout(device, reflection.get());
        badge_pipeline.init_pipeline(device, reflection.get());
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

        // 2. Bind badge pipeline and buffers
        command.set_pipeline(badge_pipeline.pipeline);
        command.set_vertex_buffer(0, geometry_vbuffer);
        command.set_index_buffer(geometry_ibuffer, GPUIndexFormat::UINT16, 0);

        // Quadrant 0 (Top-Left): draw_indirect with draw_count = 1 at offset 0
        command.draw_indirect(indirect_vbuffer, 0, 1);

        // Quadrant 1 (Top-Right): draw_indexed_indirect with draw_count = 1 at offset 0
        command.draw_indexed_indirect(indirect_ibuffer, 0, 1);

        // Quadrant 2 (Bottom-Left): Multi-draw indirect! draw_indirect with draw_count = 2 at offset 16 (sizeof(DrawIndirectArgs))
        command.draw_indirect(indirect_vbuffer, sizeof(DrawIndirectArgs), 2);

        // Quadrant 3 (Bottom-Right): draw_indexed_indirect with non-zero indirect_offset at offset 20 (sizeof(DrawIndexedIndirectArgs))
        command.draw_indexed_indirect(indirect_ibuffer, sizeof(DrawIndexedIndirectArgs), 1);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);

        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::pipeline_indirect_drawing" * doctest::description("Testing draw_indirect and draw_indexed_indirect operations."))
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
    IndirectDrawingApp(desc).run();
}
#endif

#if defined(_WIN32)
TEST_CASE("rhi::d3d12::pipeline_indirect_drawing" * doctest::description("Testing draw_indirect and draw_indexed_indirect operations."))
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
    IndirectDrawingApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::pipeline_indirect_drawing" * doctest::description("Testing draw_indirect and draw_indexed_indirect operations."))
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
    IndirectDrawingApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::pipeline_indirect_drawing" * doctest::description("Testing draw_indirect and draw_indexed_indirect operations."))
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
    IndirectDrawingApp(desc).run();
}
