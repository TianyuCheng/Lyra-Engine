#include "helper.h"

CString dynamic_uniform_program = R"""(
import lyra;

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

struct MVP
{
    float4x4 xform;
};

[lyra::dynamic]
ConstantBuffer<MVP> mvp; // dynamic uniform buffer needs additional attributes

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.position = mul(output.position, mvp.xform);
    output.color = float4(input.color, 1.0);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return input.color;
}
)""";

struct DynamicUniform
{
    glm::mat4x4 mvp;
};

struct DynamicUniformApp : public TestApp
{
    Uniform              uniform;
    Geometry             geometry;
    SimpleRenderPipeline pipeline;
    GPUBindGroup         bind_group;

    explicit DynamicUniformApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
        setup_descriptors();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        uint  width  = desc.width;
        uint  height = desc.height;
        float fovy   = 1.05f;
        float aspect = float(width) / float(height);
        auto  proj   = glm::perspective(fovy, aspect, 0.1f, 100.0f);
        auto  view   = glm::lookAt(
            glm::vec3(0.0, 0.0, 3.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        geometry = Geometry::create_triangle();

        uniform.ubuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "dynamic_uniform_buffer";
            desc.size               = sizeof(DynamicUniform) * 3;
            desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        auto data      = uniform.ubuffer.get_mapped_range<DynamicUniform>();
        data.at(0).mvp = proj * view * glm::translate(glm::mat4(1.0f), glm::vec3(-1.0, 0.0, 0.0));
        data.at(1).mvp = proj * view * glm::translate(glm::mat4(1.0f), glm::vec3(+0.0, 0.0, 0.0));
        data.at(2).mvp = proj * view * glm::translate(glm::mat4(1.0f), glm::vec3(+1.0, 0.0, 0.0));
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test";
            desc.path   = "test.slang";
            desc.source = dynamic_uniform_program;
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
        pipeline.init_vshader(device, module.get(), "vsmain");
        pipeline.init_fshader(device, module.get(), "fsmain");
        pipeline.init_playout(device, reflection.get());
        pipeline.init_pipeline(device, reflection.get());
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
            entry.buffer.size   = sizeof(DynamicUniform); // NOTE: THIS MUST NOT BE 0 WHEN DYNAMIC UNIFORM IS ENABLED

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

        // render pass info
        auto render_pass                     = GPURenderPassDescriptor{};
        render_pass.color_attachments        = color_attachment;
        render_pass.depth_stencil_attachment = {};

        // synchronization when window is enabled
        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        // commands
        command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), color_attachment_state()));
        command.begin_render_pass(render_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.set_pipeline(pipeline.pipeline);
        command.set_vertex_buffer(0, geometry.vbuffer);
        command.set_index_buffer(geometry.ibuffer, GPUIndexFormat::UINT32);
        for (uint i = 0; i < 3; i++) {
            command.set_bind_group(0, bind_group, {i * (uint)sizeof(DynamicUniform)});
            command.draw_indexed(3, 1, 0, 0, 0);
        }
        command.end_render_pass();
        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

TEST_CASE("rhi::vulkan::dynamic_uniform" * doctest::description("Rendering multiple triangles with the dynamic uniform buffer."))
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
    DynamicUniformApp(desc).run();
}

#ifdef WIN32
TEST_CASE("rhi::d3d12::dynamic_uniform" * doctest::description("Rendering multiple triangles with the dynamic uniform buffer."))
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
    DynamicUniformApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::d3d12::dynamic_uniform" * doctest::description("Rendering multiple triangles with the dynamic uniform buffer."))
{
    TestAppDescriptor desc{};
    desc.name           = "d3d12";
    desc.window         = false;
    desc.backend        = RHIBackend::METAL;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::MSL;
    desc.compile_flags  = CompileFlag::DEBUG;
    DynamicUniformApp(desc).run();
}
#endif
