#include "helper.h"

CString depth_bias_program = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float3 color    : COLOR0;
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
    output.color = float4(input.color, 1.0);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return input.color;
}
)""";

struct DepthBiasApp : public TestApp
{
    GPUTexture           dbuffer;
    GPUTextureView       dview;
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    SimpleRenderPipeline pipeline_base;
    SimpleRenderPipeline pipeline_pos_const;
    SimpleRenderPipeline pipeline_pos_slope;
    SimpleRenderPipeline pipeline_neg_const;
    SimpleRenderPipeline pipeline_neg_slope;

    explicit DepthBiasApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_depth();
        setup_buffers();
        setup_pipeline();
    }

    void setup_depth()
    {
        auto& device = RHI::get_current_device();

        dbuffer = execute([&]() {
            auto desc            = GPUTextureDescriptor{};
            desc.format          = GPUTextureFormat::DEPTH16UNORM;
            desc.size.width      = this->desc.width;
            desc.size.height     = this->desc.height;
            desc.size.depth      = 1;
            desc.array_layers    = 1;
            desc.mip_level_count = 1;
            desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT;
            desc.label           = "depth_buffer";
            return device.create_texture(desc);
        });

        dview = dbuffer.create_view();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<Vertex>   vertices;
        Vector<uint32_t> indices;

        // helper: add a quad with interpolated depth and colors (z_bottom at y0, z_top at y1)
        auto add_quad_sloped = [&](float x0, float y0, float z_bottom, float x1, float y1, float z_top, const glm::vec3& c_bottom, const glm::vec3& c_top) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            Vertex   v0{}, v1{}, v2{}, v3{};
            v0.position = {x0, y0, z_bottom};
            v0.color    = c_bottom;
            v1.position = {x1, y0, z_bottom};
            v1.color    = c_bottom;
            v2.position = {x1, y1, z_top};
            v2.color    = c_top;
            v3.position = {x0, y1, z_top};
            v3.color    = c_top;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // CCW triangles
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        // helper: add a diamond with interpolated depth and colors along vertical axis
        auto add_diamond_sloped = [&](float cx, float cy, float rw, float rh, float z_center, float dz, const glm::vec3& c_bottom, const glm::vec3& c_mid, const glm::vec3& c_top) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            Vertex   v0{}, v1{}, v2{}, v3{};
            v0.position = {cx, cy + rh, z_center - dz}; // top
            v0.color    = c_top;
            v1.position = {cx - rw, cy, z_center}; // left
            v1.color    = c_mid;
            v2.position = {cx, cy - rh, z_center + dz}; // bottom
            v2.color    = c_bottom;
            v3.position = {cx + rw, cy, z_center}; // right
            v3.color    = c_mid;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // CCW triangles: (0, 1, 2) and (0, 2, 3)
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        const glm::vec3 card_slate(0.13f, 0.15f, 0.20f);
        const glm::vec3 red(0.95f, 0.10f, 0.15f);
        const glm::vec3 emerald(0.00f, 0.90f, 0.45f);
        const glm::vec3 cyan_top(0.15f, 0.95f, 1.00f);
        const glm::vec3 cyan_bot(0.00f, 0.50f, 0.85f);
        const glm::vec3 amber(1.00f, 0.75f, 0.05f);
        const glm::vec3 magenta_top(1.00f, 0.30f, 0.85f);
        const glm::vec3 magenta_mid(0.85f, 0.15f, 0.70f);
        const glm::vec3 magenta_bot(0.65f, 0.05f, 0.50f);

        const float hw = 0.36f;
        const float hh = 0.36f;
        const float rw = 0.22f;
        const float rh = 0.22f;
        const float dz = (rh / hh) * 0.20f; // slope dz for diamond (0.1222f) matching card slope (0.3 to 0.7)

        // 1. Base passes (drawn with pipeline_base: writes depth)
        // Top-Left (Q0) Base: Flat Emerald Green card at z = 0.5 (indices 0..5)
        add_quad_sloped(-0.5f - hw, 0.5f - hh, 0.5f, -0.5f + hw, 0.5f + hh, 0.5f, emerald, emerald);

        // Top-Right (Q1) Base: Sloped Cyan gradient card from z = 0.3 (top) to 0.7 (bottom) (indices 6..11)
        add_quad_sloped(0.5f - hw, 0.5f - hh, 0.7f, 0.5f + hw, 0.5f + hh, 0.3f, cyan_bot, cyan_top);

        // Bottom-Left (Q2) Base: Flat dark card at z = 0.5 (indices 12..17)
        add_quad_sloped(-0.5f - hw, -0.5f - hh, 0.5f, -0.5f + hw, -0.5f + hh, 0.5f, card_slate, card_slate);
        // Bottom-Left (Q2) Base Red diamond at z = 0.5 (indices 18..23)
        add_diamond_sloped(-0.5f, -0.5f, rw, rh, 0.5f, 0.0f, red, red, red);

        // Bottom-Right (Q3) Base: Sloped dark card from z = 0.3 to 0.7 (indices 24..29)
        add_quad_sloped(0.5f - hw, -0.5f - hh, 0.7f, 0.5f + hw, -0.5f + hh, 0.3f, card_slate, card_slate);
        // Bottom-Right (Q3) Base Red diamond sloped from z = 0.3 to 0.7 (indices 30..35)
        add_diamond_sloped(0.5f, -0.5f, rw, rh, 0.5f, dz, red, red, red);

        // 2. Biased overlay passes:
        // Top-Left (Q0) Biased Red diamond at z = 0.5 (depth_bias > 0 -> pushed away -> fails depth test) (indices 36..41)
        add_diamond_sloped(-0.5f, 0.5f, rw, rh, 0.5f, 0.0f, red, red, red);

        // Top-Right (Q1) Biased Red diamond on slope (slope_scale > 0 -> pushed away -> fails depth test) (indices 42..47)
        add_diamond_sloped(0.5f, 0.5f, rw, rh, 0.5f, dz, red, red, red);

        // Bottom-Left (Q2) Biased Amber Gold diamond at z = 0.5 (depth_bias < 0 -> pulled closer -> overwrites Red) (indices 48..53)
        add_diamond_sloped(-0.5f, -0.5f, rw, rh, 0.5f, 0.0f, amber, amber, amber);

        // Bottom-Right (Q3) Biased Magenta gradient diamond on slope (slope_scale < 0 -> pulled closer -> overwrites Red) (indices 54..59)
        add_diamond_sloped(0.5f, -0.5f, rw, rh, 0.5f, dz, magenta_bot, magenta_mid, magenta_top);

        vbuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vertex_buffer";
            desc.size               = sizeof(Vertex) * vertices.size();
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

        auto mapped_v = vbuffer.get_mapped_range<Vertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(Vertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint32_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint32_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "depth_bias_shader";
            desc.path   = "depth_bias.slang";
            desc.source = depth_bias_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        auto init_pipe = [&](SimpleRenderPipeline& pipe, GPUCompareFunction depth_compare, int32_t depth_bias, float depth_bias_slope) {
            pipe.vstride = sizeof(Vertex);
            pipe.attributes.push_back({"position", offsetof(Vertex, position)});
            pipe.attributes.push_back({"color", offsetof(Vertex, color)});
            pipe.init_color_state(get_backbuffer_format());
            pipe.init_depth_state(GPUTextureFormat::DEPTH16UNORM);
            pipe.dsstate->depth_compare          = depth_compare;
            pipe.dsstate->depth_write_enabled    = true;
            pipe.dsstate->depth_bias             = depth_bias;
            pipe.dsstate->depth_bias_slope_scale = depth_bias_slope;
            pipe.init_vshader(device, module.get(), "vsmain");
            pipe.init_fshader(device, module.get(), "fsmain");
            pipe.init_playout(device, reflection.get());
            pipe.init_pipeline(device, reflection.get());
        };

        // 1. Base pipeline: always writes depth, zero bias
        init_pipe(pipeline_base, GPUCompareFunction::ALWAYS, 0, 0.0f);

        // 2. PosConstantBias: LESS_EQUAL compare, positive bias pushes fragment deeper (fails against existing depth)
        init_pipe(pipeline_pos_const, GPUCompareFunction::LESS_EQUAL, 8, 0.0f);

        // 3. PosSlopeBias: LESS_EQUAL compare, positive slope bias pushes sloped fragment deeper
        init_pipe(pipeline_pos_slope, GPUCompareFunction::LESS_EQUAL, 0, 2.0f);

        // 4. NegConstantBias: LESS compare, negative bias pulls fragment closer (passes against existing depth)
        init_pipe(pipeline_neg_const, GPUCompareFunction::LESS, -8, 0.0f);

        // 5. NegSlopeBias: LESS compare, negative slope bias pulls sloped fragment closer
        init_pipe(pipeline_neg_slope, GPUCompareFunction::LESS, 0, -2.0f);
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

        // 1. Draw all base cards and base diamonds with pipeline_base (writes depth, 36 indices)
        command.set_pipeline(pipeline_base.pipeline);
        command.draw_indexed(36, 1, 0, 0, 0);

        // 2. Draw Q0 Biased (Red) with positive constant bias -> fails depth test, Emerald card remains
        command.set_pipeline(pipeline_pos_const.pipeline);
        command.draw_indexed(6, 1, 36, 0, 0);

        // 3. Draw Q1 Biased (Red) with positive slope bias -> fails depth test, Cyan sloped card remains
        command.set_pipeline(pipeline_pos_slope.pipeline);
        command.draw_indexed(6, 1, 42, 0, 0);

        // 4. Draw Q2 Biased (Amber) with negative constant bias -> passes LESS depth test, overwrites Red diamond
        command.set_pipeline(pipeline_neg_const.pipeline);
        command.draw_indexed(6, 1, 48, 0, 0);

        // 5. Draw Q3 Biased (Magenta) with negative slope bias -> passes LESS depth test, overwrites Red diamond
        command.set_pipeline(pipeline_neg_slope.pipeline);
        command.draw_indexed(6, 1, 54, 0, 0);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::depth_bias" * doctest::description("Testing rasterizer depth bias and slope scale."))
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
    DepthBiasApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::depth_bias" * doctest::description("Testing rasterizer depth bias and slope scale."))
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
    DepthBiasApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::depth_bias" * doctest::description("Testing rasterizer depth bias and slope scale."))
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
    DepthBiasApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::depth_bias" * doctest::description("Testing rasterizer depth bias and slope scale."))
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
    DepthBiasApp(desc).run();
}
