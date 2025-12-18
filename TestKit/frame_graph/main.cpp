#include "helper.h"

CString frame_graph_program_pass1 = R"""(
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

CString frame_graph_program_pass2 = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

Texture2D<float4> texture;
SamplerState sampler;

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float3 color0 = texture.Sample(sampler, input.uv).rgb;
    float3 color1 = texture.Sample(sampler, float2(1.0) - input.uv).rgb;
    float3 color = color0 + color1;
    return float4(color, length(color) > 0.0 ? 1.0 : 0.0);
}
)""";

struct FrameGraphApp : public TestApp
{
    Uniform               uniform;
    Geometry              triangle;
    Geometry              quad;
    GPUSampler            sampler;
    SimpleRenderPipeline  pipeline1;
    SimpleRenderPipeline  pipeline2;
    FrameGraph::Allocator allocator;

    struct DrawPassData
    {
        FrameGraph::Resource color;
    };

    struct PostPassData
    {
        FrameGraph::Resource texture;
        FrameGraph::Resource color;
    };

    explicit FrameGraphApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_sampler();
        setup_pipeline1();
        setup_pipeline2();
    }

    void setup_buffers()
    {
        uint  width  = desc.width;
        uint  height = desc.height;
        float fovy   = 1.05f;
        float aspect = float(width) / float(height);

        quad     = Geometry::create_fullscreen();
        triangle = Geometry::create_triangle();
        uniform  = Uniform::create(fovy, aspect, glm::vec3(0.0, 0.0, 3.0));
    }

    void setup_sampler()
    {
        auto& device = RHI::get_current_device();

        sampler = execute([&] {
            auto desc           = GPUSamplerDescriptor{};
            desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
            desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
            desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
            desc.lod_max_clamp  = 1.0f;
            desc.lod_min_clamp  = 0.0f;
            desc.compare_enable = false;
            desc.compare        = GPUCompareFunction::ALWAYS;
            desc.min_filter     = GPUFilterMode::NEAREST;
            desc.mag_filter     = GPUFilterMode::NEAREST;
            desc.mipmap_filter  = GPUMipmapFilterMode::NEAREST;
            desc.max_anisotropy = 1;
            desc.label          = "sampler";
            return device.create_sampler(desc);
        });
    }

    void setup_pipeline1()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test";
            desc.path   = "test.slang";
            desc.source = frame_graph_program_pass1;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline1.vstride = sizeof(Vertex);
        pipeline1.attributes.push_back({"position", offsetof(Vertex, position)});
        pipeline1.attributes.push_back({"color", offsetof(Vertex, color)});
        pipeline1.init_color_state(GPUTextureFormat::RGBA8UNORM);
        pipeline1.init_vshader(device, module.get(), "vsmain");
        pipeline1.init_fshader(device, module.get(), "fsmain");
        pipeline1.init_playout(device, reflection.get());
        pipeline1.init_pipeline(device, reflection.get());
    }

    void setup_pipeline2()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test2";
            desc.path   = "test2.slang";
            desc.source = frame_graph_program_pass2;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline2.vstride = sizeof(Vertex);
        pipeline2.attributes.push_back({"position", offsetof(Vertex, position)});
        pipeline2.attributes.push_back({"uv", offsetof(Vertex, uv)});
        pipeline2.init_color_state(get_backbuffer_format());
        pipeline2.init_vshader(device, module.get(), "vsmain");
        pipeline2.init_fshader(device, module.get(), "fsmain");
        pipeline2.init_playout(device, reflection.get());
        pipeline2.init_pipeline(device, reflection.get());
    }

    void pass1(GPUCommandBuffer& command, GPUTextureView view)
    {
        auto& device = RHI::get_current_device();

        // create bind group
        auto bind_group = execute([&]() {
            Array<GPUBindGroupEntry, 1> entries = {};

            auto& entry         = entries.at(0);
            entry.type          = GPUBindingResourceType::BUFFER;
            entry.binding       = 0;
            entry.buffer.buffer = uniform.ubuffer;
            entry.buffer.offset = 0;
            entry.buffer.size   = 0;

            auto desc    = GPUBindGroupDescriptor{};
            desc.heap    = bheap;
            desc.layout  = pipeline1.blayouts.at(0);
            desc.entries = entries;
            return device.create_bind_group(desc);
        });

        // color attachments
        auto color_attachment        = GPURenderPassColorAttachment{};
        color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};
        color_attachment.load_op     = GPULoadOp::CLEAR;
        color_attachment.store_op    = GPUStoreOp::STORE;
        color_attachment.view        = view;

        // render pass info
        auto render_pass                     = GPURenderPassDescriptor{};
        render_pass.color_attachments        = color_attachment;
        render_pass.depth_stencil_attachment = {};

        command.begin_render_pass(render_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.set_pipeline(pipeline1.pipeline);
        command.set_vertex_buffer(0, triangle.vbuffer);
        command.set_index_buffer(triangle.ibuffer, GPUIndexFormat::UINT32);
        command.set_bind_group(0, bind_group);
        command.draw_indexed(3, 1, 0, 0, 0);
        command.end_render_pass();
    }

    void pass2(GPUCommandBuffer& command, GPUTextureView view, GPUTextureView texview)
    {
        auto& device = RHI::get_current_device();

        // create bind group
        auto bind_group = execute([&]() {
            Array<GPUBindGroupEntry, 2> entries = {};

            auto& entry0   = entries.at(0);
            entry0.type    = GPUBindingResourceType::TEXTURE;
            entry0.binding = 0;
            entry0.texture = texview;

            auto& entry1   = entries.at(1);
            entry1.type    = GPUBindingResourceType::SAMPLER;
            entry1.binding = 1;
            entry1.sampler = sampler;

            auto desc    = GPUBindGroupDescriptor{};
            desc.heap    = bheap;
            desc.layout  = pipeline2.blayouts.at(0);
            desc.entries = entries;
            return device.create_bind_group(desc);
        });

        // color attachments
        auto color_attachment        = GPURenderPassColorAttachment{};
        color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};
        color_attachment.load_op     = GPULoadOp::CLEAR;
        color_attachment.store_op    = GPUStoreOp::STORE;
        color_attachment.view        = view;

        // render pass info
        auto render_pass                     = GPURenderPassDescriptor{};
        render_pass.color_attachments        = color_attachment;
        render_pass.depth_stencil_attachment = {};

        command.begin_render_pass(render_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.set_pipeline(pipeline2.pipeline);
        command.set_vertex_buffer(0, quad.vbuffer);
        command.set_index_buffer(quad.ibuffer, GPUIndexFormat::UINT32);
        command.set_bind_group(0, bind_group);
        command.draw_indexed(6, 1, 0, 0, 0);
        command.end_render_pass();
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

        // synchronization when window is enabled
        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        FrameGraph::Builder builder;

        // first pass: draw
        auto& draw_pass = builder.create_pass("draw-pass");
        auto  draw_data = draw_pass.compile<DrawPassData>([&](auto& pass) {
            GPUTextureDescriptor descriptor{};
            descriptor.size.width      = desc.width;
            descriptor.size.height     = desc.height;
            descriptor.size.depth      = 1;
            descriptor.array_layers    = 1;
            descriptor.mip_level_count = 1;
            descriptor.sample_count    = 1;
            descriptor.format          = GPUTextureFormat::RGBA8UNORM;
            descriptor.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::RENDER_ATTACHMENT;

            DrawPassData data{};
            data.color = builder.render(builder.create<FrameGraph::Texture>(descriptor));
            return data;
        });
        draw_pass.execute([&](FrameGraph::Resources& resources, void* context) {
            auto ctx = reinterpret_cast<FrameGraphContext*>(context);
            auto tex = resources.get<FrameGraph::Texture>(draw_data.color);
            this->pass1(ctx->cmdlist, tex->view);
        });

        // second pass: post processing
        auto& post_pass = builder.create_pass("draw-pass");
        auto  post_data = post_pass.compile<PostPassData>([&](auto& pass) {
            FrameGraph::Texture texture{};
            texture.texture = backbuffer.texture;
            texture.view    = backbuffer.view;

            // directly import from backbuffer
            PostPassData data{};
            data.texture = builder.sample(draw_data.color);
            data.color   = builder.render(builder.import(texture));
            return data;
        });
        post_pass.execute([&](FrameGraph::Resources& resources, void* context) {
            auto ctx = reinterpret_cast<FrameGraphContext*>(context);
            auto tex = resources.get<FrameGraph::Texture>(draw_data.color);
            auto rt  = resources.get<FrameGraph::Texture>(post_data.color);
            this->pass2(ctx->cmdlist, rt->view, tex->view);
        });

        // third pass: present
        auto& present_pass = builder.create_pass("present-pass");
        present_pass.compile<void>([&](auto& pass) {
            // use NOP to allow manual handling of barriers
            auto _ = builder.read(post_data.color, FrameGraphReadOp::NOP);
            pass.preserve();
        });
        present_pass.execute([&](FrameGraph::Resources& resources, void* context) {
            auto ctx = reinterpret_cast<FrameGraphContext*>(context);
            auto tex = resources.get<FrameGraph::Texture>(post_data.color);
            postprocessing(ctx->cmdlist, tex->texture);
        });

        auto graph   = builder.build();
        auto context = FrameGraph::Context{device, swp, command};
        graph->execute(&context, &allocator);
        command.submit();
    }
};

TEST_CASE("rhi::vulkan::frame_graph" * doctest::description("Rendering triangles with frame graph"))
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
    FrameGraphApp(desc).run();
}

#ifdef WIN32
TEST_CASE("rhi::d3d12::frame_graph" * doctest::description("Rendering triangles with frame graph."))
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
    FrameGraphApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::frame_graph" * doctest::description("Rendering triangles with frame graph."))
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
    FrameGraphApp(desc).run();
}
#endif
