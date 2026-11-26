#include "helper.h"

CString depth_compare_ops_program = R"""(
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

struct DepthVertex
{
    glm::vec3 position;
    float     _pad = 0.0f;
    glm::vec4 color;
};

struct DepthCompareOpsApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    GPUTexture           dbuffer;
    GPUTextureView       dview;
    SimpleRenderPipeline pipeline_base_card;
    SimpleRenderPipeline pipeline_less;
    SimpleRenderPipeline pipeline_greater;
    SimpleRenderPipeline pipeline_equal;
    SimpleRenderPipeline pipeline_always;
    SimpleRenderPipeline pipeline_never;

    explicit DepthCompareOpsApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_depth();
        setup_buffers();
        setup_pipeline();
    }

    void setup_depth()
    {
        auto& device = RHI::get_current_device();

        dbuffer = execute([&]() {
            auto ddesc            = GPUTextureDescriptor{};
            ddesc.format          = GPUTextureFormat::DEPTH16UNORM;
            ddesc.size.width      = desc.width;
            ddesc.size.height     = desc.height;
            ddesc.size.depth      = 1;
            ddesc.array_layers    = 1;
            ddesc.mip_level_count = 1;
            ddesc.usage           = GPUTextureUsage::RENDER_ATTACHMENT;
            ddesc.label           = "depth_buffer";
            return device.create_texture(ddesc);
        });

        dview = dbuffer.create_view();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<DepthVertex> vertices;
        Vector<uint32_t>    indices;

        auto add_quad = [&](float x0, float y0, float x1, float y1, float z, const glm::vec4& color) {
            uint32_t    base = static_cast<uint32_t>(vertices.size());
            DepthVertex v0{}, v1{}, v2{}, v3{};
            v0.position = {x0, y0, z};
            v0.color    = color;
            v1.position = {x1, y0, z};
            v1.color    = color;
            v2.position = {x1, y1, z};
            v2.color    = color;
            v3.position = {x0, y1, z};
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

        auto add_diamond = [&](float cx, float cy, float rw, float rh, float z, const glm::vec4& color) {
            uint32_t    base = static_cast<uint32_t>(vertices.size());
            DepthVertex v0{}, v1{}, v2{}, v3{};
            v0.position = {cx, cy + rh, z}; // top
            v0.color    = color;
            v1.position = {cx - rw, cy, z}; // left
            v1.color    = color;
            v2.position = {cx, cy - rh, z}; // bottom
            v2.color    = color;
            v3.position = {cx + rw, cy, z}; // right
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

        const float hw       = 0.36f;
        const float hh       = 0.36f;
        const float rw       = 0.13f;
        const float rh       = 0.20f;
        const float x_offset = 0.16f;

        const glm::vec4 card_color{0.10f, 0.12f, 0.16f, 1.0f};
        const glm::vec4 ruby{1.00f, 0.15f, 0.25f, 1.0f};
        const glm::vec4 emerald{0.15f, 1.00f, 0.35f, 1.0f};
        const glm::vec4 sapphire{0.20f, 0.45f, 1.00f, 1.0f};
        const glm::vec4 amber{1.00f, 0.80f, 0.15f, 1.0f};
        const glm::vec4 error_black{0.0f, 0.0f, 0.0f, 1.0f};

        // 1. Base cards at z = 0.5 (indices 0..23, drawn with pipeline_base_card)
        add_quad(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh, 0.5f, card_color);
        add_quad(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, 0.5f, card_color);
        add_quad(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, 0.5f, card_color);
        add_quad(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, 0.5f, card_color);

        // 2. Q0 Top-Left (LESS): Left diamond at z = 0.3 (PASS), Right diamond at z = 0.7 (FAIL) (indices 24..35)
        add_diamond(-0.5f - x_offset, 0.5f, rw, rh, 0.3f, ruby);
        add_diamond(-0.5f + x_offset, 0.5f, rw, rh, 0.7f, error_black);

        // 3. Q1 Top-Right (GREATER): Left diamond at z = 0.7 (PASS), Right diamond at z = 0.3 (FAIL) (indices 36..47)
        add_diamond(0.5f - x_offset, 0.5f, rw, rh, 0.7f, emerald);
        add_diamond(0.5f + x_offset, 0.5f, rw, rh, 0.3f, error_black);

        // 4. Q2 Bottom-Left (EQUAL): Left diamond at z = 0.5 (PASS), Right diamond at z = 0.6 (FAIL) (indices 48..59)
        add_diamond(-0.5f - x_offset, -0.5f, rw, rh, 0.5f, sapphire);
        add_diamond(-0.5f + x_offset, -0.5f, rw, rh, 0.6f, error_black);

        // 5. Q3 Bottom-Right (ALWAYS vs NEVER): Left diamond at z = 0.9 (ALWAYS -> PASS), Right diamond at z = 0.1 (NEVER -> FAIL) (indices 60..71)
        add_diamond(0.5f - x_offset, -0.5f, rw, rh, 0.9f, amber);
        add_diamond(0.5f + x_offset, -0.5f, rw, rh, 0.1f, error_black);

        vbuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vertex_buffer";
            desc.size               = sizeof(DepthVertex) * vertices.size();
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

        auto mapped_v = vbuffer.get_mapped_range<DepthVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(DepthVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint32_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint32_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "depth_compare_shader";
            desc.path   = "depth_compare.slang";
            desc.source = depth_compare_ops_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        auto init_pipe = [&](SimpleRenderPipeline& pipe, GPUCompareFunction depth_comp, bool depth_write) {
            pipe.vstride = sizeof(DepthVertex);
            pipe.attributes.push_back({"position", offsetof(DepthVertex, position)});
            pipe.attributes.push_back({"color", offsetof(DepthVertex, color)});

            pipe.init_color_state(get_backbuffer_format());
            pipe.init_depth_state(GPUTextureFormat::DEPTH16UNORM, depth_comp, depth_write);

            pipe.init_vshader(device, module.get(), "vsmain");
            pipe.init_fshader(device, module.get(), "fsmain");
            pipe.init_playout(device, reflection.get());
            pipe.init_pipeline(device, reflection.get());
        };

        init_pipe(pipeline_base_card, GPUCompareFunction::ALWAYS, true);
        init_pipe(pipeline_less, GPUCompareFunction::LESS, false);
        init_pipe(pipeline_greater, GPUCompareFunction::GREATER, false);
        init_pipe(pipeline_equal, GPUCompareFunction::EQUAL, false);
        init_pipe(pipeline_always, GPUCompareFunction::ALWAYS, false);
        init_pipe(pipeline_never, GPUCompareFunction::NEVER, false);
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

        auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
        depth_attachment.view              = dview;
        depth_attachment.depth_clear_value = 1.0f;
        depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
        depth_attachment.depth_store_op    = GPUStoreOp::STORE;
        depth_attachment.depth_read_only   = false;

        auto render_pass                     = GPURenderPassDescriptor{};
        render_pass.color_attachments        = color_attachment;
        render_pass.depth_stencil_attachment = depth_attachment;

        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), color_attachment_texture_state()));
        command.resource_barrier(state_transition(dbuffer, undefined_texture_state(), depth_stencil_attachment_texture_state()));

        command.begin_render_pass(render_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);

        command.set_vertex_buffer(0, vbuffer);
        command.set_index_buffer(ibuffer, GPUIndexFormat::UINT32);

        // 1. Base cards (24 indices) at z = 0.5 with depth_write
        command.set_pipeline(pipeline_base_card.pipeline);
        command.draw_indexed(24, 1, 0, 0, 0);

        // 2. Q0 Top-Left: LESS test (12 indices, offset 24)
        command.set_pipeline(pipeline_less.pipeline);
        command.draw_indexed(12, 1, 24, 0, 0);

        // 3. Q1 Top-Right: GREATER test (12 indices, offset 36)
        command.set_pipeline(pipeline_greater.pipeline);
        command.draw_indexed(12, 1, 36, 0, 0);

        // 4. Q2 Bottom-Left: EQUAL test (12 indices, offset 48)
        command.set_pipeline(pipeline_equal.pipeline);
        command.draw_indexed(12, 1, 48, 0, 0);

        // 5. Q3 Bottom-Right: Left diamond (6 indices, offset 60) with ALWAYS, Right diamond (6 indices, offset 66) with NEVER
        command.set_pipeline(pipeline_always.pipeline);
        command.draw_indexed(6, 1, 60, 0, 0);
        command.set_pipeline(pipeline_never.pipeline);
        command.draw_indexed(6, 1, 66, 0, 0);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::depth_compare_ops" * doctest::description("Testing rasterizer depth compare operations."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthCompareOpsApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::depth_compare_ops" * doctest::description("Testing rasterizer depth compare operations."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthCompareOpsApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::depth_compare_ops" * doctest::description("Testing rasterizer depth compare operations."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthCompareOpsApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::depth_compare_ops" * doctest::description("Testing rasterizer depth compare operations."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthCompareOpsApp(desc).run();
}
