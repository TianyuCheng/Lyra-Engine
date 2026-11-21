#include "helper.h"

CString culling_winding_program = R"""(
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

struct CullingWindingApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    SimpleRenderPipeline pipeline_none;
    SimpleRenderPipeline pipeline_ccw_cull_back;
    SimpleRenderPipeline pipeline_cw_cull_back;
    SimpleRenderPipeline pipeline_ccw_cull_front;

    explicit CullingWindingApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<Vertex>   vertices;
        Vector<uint32_t> indices;

        // helper: add a quad with ccw triangles
        auto add_quad_ccw = [&](float x0, float y0, float x1, float y1, const glm::vec3& color) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            Vertex   v0{}, v1{}, v2{}, v3{};
            v0.position = {x0, y0, 0.0f}; // bottom-left
            v0.color    = color;
            v1.position = {x1, y0, 0.0f}; // bottom-right
            v1.color    = color;
            v2.position = {x1, y1, 0.0f}; // top-right
            v2.color    = color;
            v3.position = {x0, y1, 0.0f}; // top-left
            v3.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // ccw triangles (0, 1, 2) and (0, 2, 3)
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        // helper: add a diamond with ccw triangles
        auto add_diamond_ccw = [&](float cx, float cy, float rw, float rh, const glm::vec3& color) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            Vertex   v0{}, v1{}, v2{}, v3{};
            v0.position = {cx, cy + rh, 0.0f}; // top
            v0.color    = color;
            v1.position = {cx - rw, cy, 0.0f}; // left
            v1.color    = color;
            v2.position = {cx, cy - rh, 0.0f}; // bottom
            v2.color    = color;
            v3.position = {cx + rw, cy, 0.0f}; // right
            v3.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // ccw: (top, left, bottom) and (top, bottom, right)
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        // helper: add a diamond with cw triangles
        auto add_diamond_cw = [&](float cx, float cy, float rw, float rh, const glm::vec3& color) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            Vertex   v0{}, v1{}, v2{}, v3{};
            v0.position = {cx, cy + rh, 0.0f}; // top
            v0.color    = color;
            v1.position = {cx - rw, cy, 0.0f}; // left
            v1.color    = color;
            v2.position = {cx, cy - rh, 0.0f}; // bottom
            v2.color    = color;
            v3.position = {cx + rw, cy, 0.0f}; // right
            v3.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // cw: (top, bottom, left) and (top, right, bottom)
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 1);
            indices.push_back(base + 0);
            indices.push_back(base + 3);
            indices.push_back(base + 2);
        };

        // helper: add a triangle with vertices in given order
        auto add_triangle = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& color) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            Vertex   v0{}, v1{}, v2{};
            v0.position = p0;
            v0.color    = color;
            v1.position = p1;
            v1.color    = color;
            v2.position = p2;
            v2.color    = color;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);

            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
        };

        const glm::vec3 card_slate(0.13f, 0.15f, 0.20f);
        const glm::vec3 red(0.95f, 0.10f, 0.15f);
        const glm::vec3 emerald(0.00f, 0.90f, 0.45f);
        const glm::vec3 cyan(0.00f, 0.85f, 1.00f);
        const glm::vec3 amber(1.00f, 0.75f, 0.05f);
        const glm::vec3 magenta(0.95f, 0.20f, 0.70f);

        const float hw = 0.36f;
        const float hh = 0.36f;

        // 1. Base card backgrounds (drawn with pipeline_none: no culling)
        // Top-Left card: indices 0..5
        add_quad_ccw(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh, card_slate);
        // Top-Right card: indices 6..11
        add_quad_ccw(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, card_slate);
        // Bottom-Left card: indices 12..17
        add_quad_ccw(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, card_slate);
        // Bottom-Right card: indices 18..23
        add_quad_ccw(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, card_slate);

        // 2. Base motifs for each quadrant (drawn with pipeline_none: no culling)
        // Q0 base: Red upward triangle (indices 24..26)
        add_triangle({-0.5f, 0.5f + 0.24f, 0.0f}, {-0.5f - 0.24f, 0.5f - 0.20f, 0.0f}, {-0.5f + 0.24f, 0.5f - 0.20f, 0.0f}, red);
        // Q1 base: Cyan diamond (indices 27..32)
        add_diamond_ccw(0.5f, 0.5f, 0.24f, 0.24f, cyan);
        // Q2 base: Red downward triangle (indices 33..35)
        add_triangle({-0.5f, -0.5f - 0.24f, 0.0f}, {-0.5f - 0.24f, -0.5f + 0.20f, 0.0f}, {-0.5f + 0.24f, -0.5f + 0.20f, 0.0f}, red);
        // Q3 base: Magenta diamond (indices 36..41)
        add_diamond_ccw(0.5f, -0.5f, 0.24f, 0.24f, magenta);

        // 3. Test motifs:
        // Q0 test: CCW Emerald triangle (front_face = CCW, cull_mode = BACK)
        // Front-facing -> NOT culled -> overwrites Red base (indices 42..44)
        add_triangle({-0.5f, 0.5f + 0.24f, 0.0f}, {-0.5f - 0.24f, 0.5f - 0.20f, 0.0f}, {-0.5f + 0.24f, 0.5f - 0.20f, 0.0f}, emerald);

        // Q1 test: CW Red diamond (front_face = CCW, cull_mode = BACK)
        // Back-facing -> culled -> Cyan base remains (indices 45..50)
        add_diamond_cw(0.5f, 0.5f, 0.24f, 0.24f, red);

        // Q2 test: CW Amber downward triangle (front_face = CW, cull_mode = BACK)
        // Vertices ordered 6 o'clock (bottom) -> 10 o'clock (top-left) -> 2 o'clock (top-right) = CW
        // Front-facing because front_face = CW -> NOT culled -> overwrites Red base (indices 51..53)
        add_triangle({-0.5f, -0.5f - 0.24f, 0.0f}, {-0.5f - 0.24f, -0.5f + 0.20f, 0.0f}, {-0.5f + 0.24f, -0.5f + 0.20f, 0.0f}, amber);

        // Q3 test: CCW Red diamond (front_face = CCW, cull_mode = FRONT)
        // Front-facing -> culled by FRONT culling -> Magenta base remains (indices 54..59)
        add_diamond_ccw(0.5f, -0.5f, 0.24f, 0.24f, red);

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
            desc.module = "culling_winding_shader";
            desc.path   = "culling_winding.slang";
            desc.source = culling_winding_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        auto init_pipe = [&](SimpleRenderPipeline& pipe, GPUFrontFace front_face, GPUCullMode cull_mode) {
            pipe.vstride = sizeof(Vertex);
            pipe.attributes.push_back({"position", offsetof(Vertex, position)});
            pipe.attributes.push_back({"color", offsetof(Vertex, color)});
            pipe.init_color_state(get_backbuffer_format());

            GPUPrimitiveState prim{};
            prim.topology   = GPUPrimitiveTopology::TRIANGLE_LIST;
            prim.front_face = front_face;
            prim.cull_mode  = cull_mode;
            pipe.primitive  = prim;

            pipe.init_vshader(device, module.get(), "vsmain");
            pipe.init_fshader(device, module.get(), "fsmain");
            pipe.init_playout(device, reflection.get());
            pipe.init_pipeline(device, reflection.get());
        };

        // 1. Base pipeline: no culling
        init_pipe(pipeline_none, GPUFrontFace::CCW, GPUCullMode::NONE);

        // 2. CCW front face, cull back
        init_pipe(pipeline_ccw_cull_back, GPUFrontFace::CCW, GPUCullMode::BACK);

        // 3. CW front face, cull back
        init_pipe(pipeline_cw_cull_back, GPUFrontFace::CW, GPUCullMode::BACK);

        // 4. CCW front face, cull front
        init_pipe(pipeline_ccw_cull_front, GPUFrontFace::CCW, GPUCullMode::FRONT);
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

        command.set_vertex_buffer(0, vbuffer);
        command.set_index_buffer(ibuffer, GPUIndexFormat::UINT32);

        // 1. Draw all base cards and base motifs without culling (42 indices)
        command.set_pipeline(pipeline_none.pipeline);
        command.draw_indexed(42, 1, 0, 0, 0);

        // 2. Q0 & Q1 test motifs (front_face = CCW, cull_mode = BACK)
        command.set_pipeline(pipeline_ccw_cull_back.pipeline);
        command.draw_indexed(3, 1, 42, 0, 0); // Q0: CCW Emerald triangle passes -> overwrites Red
        command.draw_indexed(6, 1, 45, 0, 0); // Q1: CW Red diamond culled -> Cyan base remains

        // 3. Q2 test motif (front_face = CW, cull_mode = BACK)
        command.set_pipeline(pipeline_cw_cull_back.pipeline);
        command.draw_indexed(3, 1, 51, 0, 0); // Q2: CW Amber triangle passes -> overwrites Red

        // 4. Q3 test motif (front_face = CCW, cull_mode = FRONT)
        command.set_pipeline(pipeline_ccw_cull_front.pipeline);
        command.draw_indexed(6, 1, 54, 0, 0); // Q3: CCW Red diamond culled -> Magenta base remains

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::culling_winding" * doctest::description("Testing rasterizer face culling and front-face winding."))
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
    CullingWindingApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::culling_winding" * doctest::description("Testing rasterizer face culling and front-face winding."))
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
    CullingWindingApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::culling_winding" * doctest::description("Testing rasterizer face culling and front-face winding."))
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
    CullingWindingApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::culling_winding" * doctest::description("Testing rasterizer face culling and front-face winding."))
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
    CullingWindingApp(desc).run();
}
