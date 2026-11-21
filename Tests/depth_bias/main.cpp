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

        auto add_quad = [&](float x0, float y0, float z0, float x1, float y1, float z1, const glm::vec3& color) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            Vertex   v0{}, v1{}, v2{}, v3{};
            v0.position = {x0, y0, z0};
            v0.color    = color;
            v1.position = {x1, y0, z1};
            v1.color    = color;
            v2.position = {x1, y1, z1};
            v2.color    = color;
            v3.position = {x0, y1, z0};
            v3.color    = color;

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

        const glm::vec3 green(0.0f, 1.0f, 0.0f);
        const glm::vec3 red(1.0f, 0.0f, 0.0f);

        // Top-Left (x in [-1, 0], y in [0, 1]): Constant Positive Depth Bias
        // Base (Green) at z = 0.5; Biased (Red) at z = 0.5 with depth_bias > 0 (pushes Red away, Green wins)
        add_quad(-1.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.5f, green); // index offset 0
        add_quad(-1.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.5f, red);   // index offset 6

        // Top-Right (x in [0, 1], y in [0, 1]): Slope-Scale Positive Depth Bias
        // Base (Green) tilted from z = 0.3 to 0.7; Biased (Red) same slope with slope_scale > 0 (pushes Red away, Green wins)
        add_quad(0.0f, 0.0f, 0.3f, 1.0f, 1.0f, 0.7f, green); // index offset 12
        add_quad(0.0f, 0.0f, 0.3f, 1.0f, 1.0f, 0.7f, red);   // index offset 18

        // Bottom-Left (x in [-1, 0], y in [-1, 0]): Constant Negative Depth Bias
        // Base (Red) at z = 0.5; Biased (Green) at z = 0.5 with depth_bias < 0 and LESS compare (pulls Green closer, Green wins)
        add_quad(-1.0f, -1.0f, 0.5f, 0.0f, 0.0f, 0.5f, red);   // index offset 24
        add_quad(-1.0f, -1.0f, 0.5f, 0.0f, 0.0f, 0.5f, green); // index offset 30

        // Bottom-Right (x in [0, 1], y in [-1, 0]): Slope-Scale Negative Depth Bias
        // Base (Red) tilted from z = 0.3 to 0.7; Biased (Green) same slope with slope_scale < 0 and LESS compare (pulls Green closer, Green wins)
        add_quad(0.0f, -1.0f, 0.3f, 1.0f, 0.0f, 0.7f, red);   // index offset 36
        add_quad(0.0f, -1.0f, 0.3f, 1.0f, 0.0f, 0.7f, green); // index offset 42

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
            pipe.dsstate->depth_compare       = depth_compare;
            pipe.dsstate->depth_write_enabled = true;
            pipe.dsstate->depth_bias          = depth_bias;
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
        color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
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

        // 1. Draw base quads with pipeline_base (writes depth)
        command.set_pipeline(pipeline_base.pipeline);
        command.draw_indexed(6, 1, 0, 0, 0);   // Top-Left Base (Green)
        command.draw_indexed(6, 1, 12, 0, 0);  // Top-Right Base (Green)
        command.draw_indexed(6, 1, 24, 0, 0);  // Bottom-Left Base (Red)
        command.draw_indexed(6, 1, 36, 0, 0);  // Bottom-Right Base (Red)

        // 2. Draw Top-Left Biased (Red) with positive constant bias -> fails depth test, Green remains
        command.set_pipeline(pipeline_pos_const.pipeline);
        command.draw_indexed(6, 1, 6, 0, 0);

        // 3. Draw Top-Right Biased (Red) with positive slope bias -> fails depth test, Green remains
        command.set_pipeline(pipeline_pos_slope.pipeline);
        command.draw_indexed(6, 1, 18, 0, 0);

        // 4. Draw Bottom-Left Biased (Green) with negative constant bias -> passes LESS depth test, overwrites Red
        command.set_pipeline(pipeline_neg_const.pipeline);
        command.draw_indexed(6, 1, 30, 0, 0);

        // 5. Draw Bottom-Right Biased (Green) with negative slope bias -> passes LESS depth test, overwrites Red
        command.set_pipeline(pipeline_neg_slope.pipeline);
        command.draw_indexed(6, 1, 42, 0, 0);

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
