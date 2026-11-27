#include "helper.h"

CString depth_test_program = R"""(
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

struct Camera
{
    float4x4 proj;
    float4x4 view;
};

ConstantBuffer<Camera> camera;

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0), camera.view), camera.proj);
    output.color = float4(input.color, 1.0);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return input.color;
}
)""";

struct DepthTestApp : public TestApp
{
    Uniform              uniform;
    Geometry             geometry;
    GPUTexture           dbuffer;
    GPUTextureView       dview;
    SimpleRenderPipeline pipeline;
    GPUBindGroup         bind_group;

    explicit DepthTestApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_depth();
        setup_buffers();
        setup_pipeline();
        setup_descriptors();
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

        test_depth32float_readonly(device);
    }

    void test_depth32float_readonly(GPUDevice& device)
    {
        // verifies non-stencil depth format (DEPTH32FLOAT) with read-only DSV and render pass access
        auto d32_texture = execute([&]() {
            auto desc            = GPUTextureDescriptor{};
            desc.format          = GPUTextureFormat::DEPTH32FLOAT;
            desc.size.width      = this->desc.width;
            desc.size.height     = this->desc.height;
            desc.size.depth      = 1;
            desc.array_layers    = 1;
            desc.mip_level_count = 1;
            desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT;
            desc.label           = "d32_readonly_test";
            return device.create_texture(desc);
        });

        auto d32_view = d32_texture.create_view();

        auto cmd = execute([&]() {
            auto desc  = GPUCommandBufferDescriptor{};
            desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        cmd.resource_barrier(state_transition(d32_texture, undefined_texture_state(), depth_stencil_read_texture_state()));

        auto d32_attachment              = GPURenderPassDepthStencilAttachment{};
        d32_attachment.view              = d32_view;
        d32_attachment.depth_clear_value = 1.0f;
        d32_attachment.depth_load_op     = GPULoadOp::LOAD;
        d32_attachment.depth_store_op    = GPUStoreOp::STORE;
        d32_attachment.depth_read_only   = true;

        auto render_pass                     = GPURenderPassDescriptor{};
        render_pass.depth_stencil_attachment = d32_attachment;

        cmd.begin_render_pass(render_pass);
        cmd.end_render_pass();
        cmd.submit();

        device.wait();

        d32_view.destroy();
        d32_texture.destroy();
    }

    void setup_buffers()
    {
        uint  width  = desc.width;
        uint  height = desc.height;
        float fovy   = 1.05f;
        float aspect = float(width) / float(height);

        geometry = Geometry::create_overlapping_triangles();
        uniform  = Uniform::create(fovy, aspect, glm::vec3(0.0, 0.0, 3.0));
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test";
            desc.path   = "test.slang";
            desc.source = depth_test_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(Vertex);
        pipeline.attributes.push_back({"position", offsetof(Vertex, position)});
        pipeline.attributes.push_back({"color", offsetof(Vertex, color)});
        pipeline.init_color_state(get_backbuffer_format());
        pipeline.init_depth_state(GPUTextureFormat::DEPTH16UNORM);
        pipeline.init_vshader(device, module->get(), "vsmain");
        pipeline.init_fshader(device, module->get(), "fsmain");
        pipeline.init_playout(device, reflection->get());
        pipeline.init_pipeline(device, reflection->get());
    }

    void setup_descriptors()
    {
        auto& device = RHI::get_current_device();

        // create bind group
        bind_group = execute([&]() {
            Array<GPUBindGroupEntry, 1> entries = {};

            auto& entry         = entries.at(0);
            entry.type          = GPUResourceType::BUFFER;
            entry.binding       = 0;
            entry.buffer.buffer = uniform.ubuffer;
            entry.buffer.offset = 0;
            entry.buffer.size   = 0;

            auto desc    = GPUBindGroupDescriptor{};
            desc.heap    = bheap;
            desc.layout  = pipeline.blayouts.at(0);
            desc.entries = entries;
            return device.create_bind_group(desc);
        });
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        // create command buffer
        auto command = execute([&]() {
            auto desc  = GPUCommandBufferDescriptor{};
            desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        // color attachments
        auto color_attachment        = GPURenderPassColorAttachment{};
        color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};
        color_attachment.load_op     = GPULoadOp::CLEAR;
        color_attachment.store_op    = GPUStoreOp::STORE;
        color_attachment.view        = backbuffer.view;

        auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
        depth_attachment.view              = dview;
        depth_attachment.depth_clear_value = 1.0f;
        depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
        depth_attachment.depth_store_op    = GPUStoreOp::STORE;
        depth_attachment.depth_read_only   = false;

        // render pass info
        auto render_pass                     = GPURenderPassDescriptor{};
        render_pass.color_attachments        = color_attachment;
        render_pass.depth_stencil_attachment = depth_attachment;

        // synchronization when window is enabled
        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        // commands
        command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), color_attachment_texture_state()));
        command.resource_barrier(state_transition(dbuffer, undefined_texture_state(), depth_stencil_attachment_texture_state()));
        command.begin_render_pass(render_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.set_pipeline(pipeline.pipeline);
        command.set_vertex_buffer(0, geometry.vbuffer);
        command.set_index_buffer(geometry.ibuffer, GPUIndexFormat::UINT32);
        command.set_bind_group(0, bind_group);
        command.draw_indexed(6, 1, 0, 0, 0);
        command.end_render_pass();

        // pass 2: verify read-only depth render pass access and barrier layout matching on dbuffer
        command.resource_barrier(state_transition(dbuffer, depth_stencil_attachment_texture_state(), depth_stencil_read_texture_state()));

        auto ro_depth_attachment            = depth_attachment;
        ro_depth_attachment.depth_load_op   = GPULoadOp::LOAD;
        ro_depth_attachment.depth_store_op  = GPUStoreOp::STORE;
        ro_depth_attachment.depth_read_only = true;

        auto ro_color_attachment    = color_attachment;
        ro_color_attachment.load_op = GPULoadOp::LOAD;

        auto ro_render_pass                     = GPURenderPassDescriptor{};
        ro_render_pass.color_attachments        = ro_color_attachment;
        ro_render_pass.depth_stencil_attachment = ro_depth_attachment;

        command.begin_render_pass(ro_render_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::depth_test" * doctest::description("Rendering a triangle with depth test enabled."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthTestApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::depth_test" * doctest::description("Rendering a triangle with depth test enabled."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthTestApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::depth_test" * doctest::description("Rendering a triangle with depth test enabled."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthTestApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::depth_test" * doctest::description("Rendering a triangle with depth test enabled."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    DepthTestApp(desc).run();
}
