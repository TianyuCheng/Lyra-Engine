#include "helper.h"

CString graphics_pipeline_program = R"""(
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

struct GraphicsPipelineApp : public TestApp
{
    Uniform              uniform;
    Geometry             geometry;
    SimpleRenderPipeline pipeline;
    GPUBindGroup         bind_group;

    explicit GraphicsPipelineApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
        setup_descriptors();
    }

    void setup_buffers()
    {
        uint  width  = desc.width;
        uint  height = desc.height;
        float fovy   = 1.05f;
        float aspect = float(width) / float(height);

        geometry = Geometry::create_triangle();
        uniform  = Uniform::create(fovy, aspect, glm::vec3(0.0, 0.0, 3.0));
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test";
            desc.path   = "test.slang";
            desc.source = graphics_pipeline_program;
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

        test_vertex_attribute_semantics(device);
    }

    void test_vertex_attribute_semantics(GPUDevice& device)
    {
        CString complex_shader_program = R"""(
struct ComplexVertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD0;
    float4 tangent  : TANGENT;
    float4 color    : COLOR1;
};

struct ComplexVertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
ComplexVertexOutput vs_complex(ComplexVertexInput input)
{
    ComplexVertexOutput output;
    output.position = float4(input.position + input.normal + float3(input.texcoord, 0.0) + input.tangent.xyz + input.color.xyz, 1.0);
    return output;
}

[shader("fragment")]
float4 fs_complex(ComplexVertexOutput input) : SV_Target
{
    return float4(1.0, 1.0, 1.0, 1.0);
}
)""";

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test_complex";
            desc.path   = "test_complex.slang";
            desc.source = complex_shader_program;
            return compiler->compile(desc);
        });

        auto vs_code = module->get_shader_blob("vs_complex");
        auto fs_code = module->get_shader_blob("fs_complex");

        GPUShaderModuleDescriptor vs_desc{};
        vs_desc.label = "vs_complex";
        vs_desc.data  = vs_code->data;
        vs_desc.size  = vs_code->size;
        auto vs_mod   = device.create_shader_module(vs_desc);

        GPUShaderModuleDescriptor fs_desc{};
        fs_desc.label = "fs_complex";
        fs_desc.data  = fs_code->data;
        fs_desc.size  = fs_code->size;
        auto fs_mod   = device.create_shader_module(fs_desc);

        GPUPipelineLayoutDescriptor playout_desc{};
        auto                        playout = device.create_pipeline_layout(playout_desc);

        // test single buffer layout with various HLSL semantics:
        // - POSITION (location 0)
        // - NORMAL (location 1, semantic without digits defaulting to semantic index 0)
        // - TEXCOORD0 (location 2, trailing digit '0' stripped from name and placed in semantic index)
        // - TANGENT (location 3)
        // - COLOR1 (location 4, trailing digit '1' placed in semantic index)
        Array<GPUVertexAttribute, 5> attrs{};
        attrs[0] = {GPUVertexFormat::FLOAT32x3, 0, 0, "POSITION"};
        attrs[1] = {GPUVertexFormat::FLOAT32x3, 12, 1, "NORMAL"};
        attrs[2] = {GPUVertexFormat::FLOAT32x2, 24, 2, "TEXCOORD0"};
        attrs[3] = {GPUVertexFormat::FLOAT32x4, 32, 3, "TANGENT"};
        attrs[4] = {GPUVertexFormat::FLOAT32x4, 48, 4, "COLOR1"};

        GPUVertexBufferLayout buffer_layout{};
        buffer_layout.array_stride = 64;
        buffer_layout.step_mode    = GPUVertexStepMode::VERTEX;
        buffer_layout.attributes   = attrs;

        Array<GPUColorTargetState, 1> color_targets{};
        color_targets[0].format = get_backbuffer_format();

        GPURenderPipelineDescriptor pdesc{};
        pdesc.layout               = playout;
        pdesc.primitive.topology   = GPUPrimitiveTopology::TRIANGLE_LIST;
        pdesc.vertex.module        = vs_mod;
        pdesc.vertex.entry_point   = "vs_complex";
        pdesc.vertex.buffers       = buffer_layout;
        pdesc.fragment.module      = fs_mod;
        pdesc.fragment.entry_point = "fs_complex";
        pdesc.fragment.targets     = color_targets;

        auto test_pipeline1 = device.create_render_pipeline(pdesc);
        REQUIRE(test_pipeline1.valid());
        test_pipeline1.destroy();

        // test multi-buffer layout splitting attributes across multiple vertex buffers
        Array<GPUVertexAttribute, 3> buf0_attrs{};
        buf0_attrs[0] = {GPUVertexFormat::FLOAT32x3, 0, 0, "POSITION"};
        buf0_attrs[1] = {GPUVertexFormat::FLOAT32x3, 12, 1, "NORMAL"};
        buf0_attrs[2] = {GPUVertexFormat::FLOAT32x2, 24, 2, "TEXCOORD0"};

        Array<GPUVertexAttribute, 2> buf1_attrs{};
        buf1_attrs[0] = {GPUVertexFormat::FLOAT32x4, 0, 3, "TANGENT"};
        buf1_attrs[1] = {GPUVertexFormat::FLOAT32x4, 16, 4, "COLOR1"};

        Array<GPUVertexBufferLayout, 2> multi_buffer_layouts{};
        multi_buffer_layouts[0].array_stride = 32;
        multi_buffer_layouts[0].step_mode    = GPUVertexStepMode::VERTEX;
        multi_buffer_layouts[0].attributes   = buf0_attrs;
        multi_buffer_layouts[1].array_stride = 32;
        multi_buffer_layouts[1].step_mode    = GPUVertexStepMode::VERTEX;
        multi_buffer_layouts[1].attributes   = buf1_attrs;

        pdesc.vertex.buffers = multi_buffer_layouts;
        auto test_pipeline2  = device.create_render_pipeline(pdesc);
        REQUIRE(test_pipeline2.valid());
        test_pipeline2.destroy();

        playout.destroy();
        fs_mod.destroy();
        vs_mod.destroy();
    }

    void test_bind_group_heap_multipool(GPUDevice& device)
    {
        CString heap_shader_program = R"""(
struct Camera
{
    float4x4 proj;
    float4x4 view;
};

struct HeapParams
{
    ConstantBuffer<Camera> camera;
    SamplerState           sampler;
};
ParameterBlock<HeapParams> params;

[shader("vertex")]
float4 vs_heap() : SV_Position { return float4(0, 0, 0, 1); }

[shader("fragment")]
float4 fs_heap() : SV_Target { return float4(1, 1, 1, 1); }
)""";

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test_heap";
            desc.path   = "test_heap.slang";
            desc.source = heap_shader_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vs_heap"},
            {*module, "fs_heap"},
        });

        auto blayouts = reflection->get_bind_group_layouts();
        REQUIRE(!blayouts.empty());
        auto blayout = device.create_bind_group_layout(blayouts[0]);
        REQUIRE(blayout.valid());

        // create heap with small page size of 4 to force multi-pool allocations
        GPUBindGroupHeapDescriptor hdesc{};
        hdesc.page_size = 4;
        auto test_heap  = device.create_bind_group_heap(hdesc);
        REQUIRE(test_heap.valid());

        GPUSamplerDescriptor sdesc{};
        sdesc.label  = "heap_test_sampler";
        auto sampler = device.create_sampler(sdesc);
        REQUIRE(sampler.valid());

        Array<GPUBindGroupEntry, 2> bg_entries{};
        bg_entries[0].binding       = 0;
        bg_entries[0].type          = GPUResourceType::BUFFER;
        bg_entries[0].buffer.buffer = uniform.ubuffer;
        bg_entries[0].buffer.offset = 0;
        bg_entries[0].buffer.size   = 0;

        bg_entries[1].binding = 1;
        bg_entries[1].type    = GPUResourceType::SAMPLER;
        bg_entries[1].sampler = sampler;

        GPUBindGroupDescriptor bg_desc{};
        bg_desc.heap    = test_heap;
        bg_desc.layout  = blayout;
        bg_desc.entries = bg_entries;

        // allocate 16 bind groups spanning multiple pools
        Vector<GPUBindGroup> groups;
        groups.reserve(16);
        for (usize i = 0; i < 16; ++i) {
            auto bg = device.create_bind_group(bg_desc);
            REQUIRE(bg.valid());
            groups.push_back(bg);
        }

        blayout.destroy();
        sampler.destroy();
        test_heap.destroy();
    }

    void setup_descriptors()
    {
        auto& device = RHI::get_current_device();

        test_bind_group_heap_multipool(device);

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
        command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), color_attachment_texture_state()));
        command.begin_render_pass(render_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);
        command.set_pipeline(pipeline.pipeline);
        command.set_vertex_buffer(0, geometry.vbuffer);
        command.set_index_buffer(geometry.ibuffer, GPUIndexFormat::UINT32);
        command.set_bind_group(0, bind_group);
        command.draw_indexed(3, 1, 0, 0, 0);
        command.end_render_pass();
        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::pipeline_basic" * doctest::description("Rendering a triangle with the most basic graphics pipeline."))
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
    GraphicsPipelineApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::pipeline_basic" * doctest::description("Rendering a triangle with the most basic graphics pipeline."))
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
    GraphicsPipelineApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::pipeline_basic" * doctest::description("Rendering a triangle with the most basic graphics pipeline."))
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
    GraphicsPipelineApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::pipeline_basic" * doctest::description("Rendering a triangle with the software graphics pipeline."))
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
    GraphicsPipelineApp(desc).run();
}
