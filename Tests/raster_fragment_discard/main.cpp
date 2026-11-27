#include "helper.h"

CString fragment_discard_program = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float  mode     : TEXCOORD1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float  mode     : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texcoord = input.texcoord;
    output.mode     = input.mode;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float2 uv = input.texcoord;
    int mode = int(input.mode);

    if (mode == 0) {
        // Mode 0: Circle cutout
        float2 c = uv - float2(0.5, 0.5);
        if (dot(c, c) < 0.10) {
            discard;
        }
        return float4(1.0, 0.25, 0.30, 1.0); // Coral
    } else if (mode == 1) {
        // Mode 1: Checkerboard cutout
        int cx = int(uv.x * 6.0);
        int cy = int(uv.y * 6.0);
        if (((cx ^ cy) & 1) != 0) {
            discard;
        }
        return float4(0.25, 0.85, 0.40, 1.0); // Mint green
    } else if (mode == 2) {
        // Mode 2: Horizontal stripes cutout
        if (frac(uv.y * 6.0) < 0.45) {
            discard;
        }
        return float4(0.30, 0.60, 1.0, 1.0); // Sky blue
    } else {
        // Mode 3: Diamond cutout
        if ((abs(uv.x - 0.5) + abs(uv.y - 0.5)) < 0.32) {
            discard;
        }
        return float4(1.0, 0.85, 0.20, 1.0); // Golden yellow
    }
}
)""";

struct DiscardVertex
{
    glm::vec3 position;
    glm::vec2 texcoord;
    float     mode;
};

struct FragmentDiscardApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    SimpleRenderPipeline pipeline;

    explicit FragmentDiscardApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_geometry();
        setup_pipeline();
    }

    void setup_geometry()
    {
        auto& device = RHI::get_current_device();

        Vector<DiscardVertex> vertices;
        Vector<uint16_t>      indices;

        auto add_quad = [&](float x0, float y0, float x1, float y1, float mode) {
            uint16_t      base = static_cast<uint16_t>(vertices.size());
            DiscardVertex v00{}, v10{}, v11{}, v01{};
            v00.position = {x0, y0, 0.0f};
            v00.texcoord = {0.0f, 0.0f};
            v00.mode     = mode;

            v10.position = {x1, y0, 0.0f};
            v10.texcoord = {1.0f, 0.0f};
            v10.mode     = mode;

            v11.position = {x1, y1, 0.0f};
            v11.texcoord = {1.0f, 1.0f};
            v11.mode     = mode;

            v01.position = {x0, y1, 0.0f};
            v01.texcoord = {0.0f, 1.0f};
            v01.mode     = mode;

            vertices.push_back(v00);
            vertices.push_back(v10);
            vertices.push_back(v11);
            vertices.push_back(v01);

            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        const float hw = 0.38f;
        const float hh = 0.38f;

        // Q0: Top-Left (Circle cutout)
        add_quad(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh, 0.0f);
        // Q1: Top-Right (Checker cutout)
        add_quad(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, 1.0f);
        // Q2: Bottom-Left (Stripes cutout)
        add_quad(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, 2.0f);
        // Q3: Bottom-Right (Diamond cutout)
        add_quad(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, 3.0f);

        vbuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "discard_vbuffer";
            bdesc.size               = sizeof(DiscardVertex) * vertices.size();
            bdesc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        ibuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "discard_ibuffer";
            bdesc.size               = sizeof(uint16_t) * indices.size();
            bdesc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        auto mapped_v = vbuffer.get_mapped_range<DiscardVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(DiscardVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint16_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&] {
            auto cdesc   = CompileDescriptor{};
            cdesc.module = "fragment_discard_shader";
            cdesc.path   = "fragment_discard.slang";
            cdesc.source = fragment_discard_program;
            return compiler->compile(cdesc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(DiscardVertex);
        pipeline.attributes.push_back({"position", offsetof(DiscardVertex, position)});
        pipeline.attributes.push_back({"texcoord", offsetof(DiscardVertex, texcoord)});
        pipeline.attributes.push_back({"mode", offsetof(DiscardVertex, mode)});

        GPUPrimitiveState prim{};
        prim.topology      = GPUPrimitiveTopology::TRIANGLE_LIST;
        prim.cull_mode     = GPUCullMode::NONE;
        pipeline.primitive = prim;

        pipeline.init_color_state(get_backbuffer_format());
        pipeline.init_vshader(device, module->get(), "vsmain");
        pipeline.init_fshader(device, module->get(), "fsmain");
        pipeline.init_playout(device, reflection->get());
        pipeline.init_pipeline(device, reflection->get());
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        auto command = execute([&] {
            auto cdesc  = GPUCommandBufferDescriptor{};
            cdesc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(cdesc);
        });

        // clear to navy blue background so discarded fragments clearly show background
        auto color_attachment        = GPURenderPassColorAttachment{};
        color_attachment.clear_value = GPUColor{0.08f, 0.10f, 0.18f, 1.0f};
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

        command.set_pipeline(pipeline.pipeline);
        command.set_vertex_buffer(0, vbuffer);
        command.set_index_buffer(ibuffer, GPUIndexFormat::UINT16, 0);
        // 4 quads * 6 indices each = 24 indices
        command.draw_indexed(24, 1, 0, 0, 0);

        command.end_render_pass();
        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::raster_fragment_discard" * doctest::description("Testing fragment shader discard execution."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    FragmentDiscardApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::raster_fragment_discard" * doctest::description("Testing fragment shader discard execution."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    FragmentDiscardApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::raster_fragment_discard" * doctest::description("Testing fragment shader discard execution."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    FragmentDiscardApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::raster_fragment_discard" * doctest::description("Testing fragment shader discard execution."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    FragmentDiscardApp(desc).run();
}
