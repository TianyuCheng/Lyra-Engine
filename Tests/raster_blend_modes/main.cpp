#include "helper.h"

CString blend_modes_program = R"""(
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

struct BlendVertex
{
    glm::vec3 position;
    float     _pad = 0.0f;
    glm::vec4 color;
};

struct BlendModesApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    SimpleRenderPipeline pipeline_opaque;
    SimpleRenderPipeline pipeline_alpha_blend;
    SimpleRenderPipeline pipeline_additive;
    SimpleRenderPipeline pipeline_multiply;
    SimpleRenderPipeline pipeline_subtractive;

    explicit BlendModesApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_pipeline();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        Vector<BlendVertex> vertices;
        Vector<uint32_t>    indices;

        // helper: add a quad with ccw triangles
        auto add_quad = [&](float x0, float y0, float x1, float y1, const glm::vec4& c_bottom, const glm::vec4& c_top) {
            uint32_t    base = static_cast<uint32_t>(vertices.size());
            BlendVertex v0{}, v1{}, v2{}, v3{};
            v0.position = {x0, y0, 0.0f}; // bottom-left
            v0.color    = c_bottom;
            v1.position = {x1, y0, 0.0f}; // bottom-right
            v1.color    = c_bottom;
            v2.position = {x1, y1, 0.0f}; // top-right
            v2.color    = c_top;
            v3.position = {x0, y1, 0.0f}; // top-left
            v3.color    = c_top;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // ccw triangles: (0, 1, 2) and (0, 2, 3)
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        };

        // helper: add a diamond with ccw triangles
        auto add_diamond = [&](float cx, float cy, float rw, float rh, const glm::vec4& color) {
            uint32_t    base = static_cast<uint32_t>(vertices.size());
            BlendVertex v0{}, v1{}, v2{}, v3{};
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

        const float hw = 0.36f;
        const float hh = 0.36f;
        const float rw = 0.22f;
        const float rh = 0.22f;

        // 1. Base passes (drawn with pipeline_opaque: no blending)
        // Q0 (Top-Left): Dual-split card: left half ocean blue, right half golden amber (indices 0..11)
        add_quad(-0.5f - hw, 0.5f - hh, -0.5f, 0.5f + hh, {0.10f, 0.30f, 0.80f, 1.0f}, {0.10f, 0.30f, 0.80f, 1.0f});
        add_quad(-0.5f, 0.5f - hh, -0.5f + hw, 0.5f + hh, {0.95f, 0.70f, 0.10f, 1.0f}, {0.95f, 0.70f, 0.10f, 1.0f});

        // Q1 (Top-Right): Dark midnight violet card (indices 12..17)
        add_quad(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, {0.10f, 0.05f, 0.20f, 1.0f}, {0.10f, 0.05f, 0.20f, 1.0f});

        // Q2 (Bottom-Left): Sunset gradient card (Orange-Red at bottom to Gold at top) (indices 18..23)
        add_quad(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, {0.90f, 0.25f, 0.10f, 1.0f}, {1.00f, 0.95f, 0.70f, 1.0f});

        // Q3 (Bottom-Right): Radiant amber-orange card (indices 24..29)
        add_quad(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, {1.00f, 0.80f, 0.20f, 1.0f}, {1.00f, 0.80f, 0.20f, 1.0f});

        // 2. Q0 Alpha Blend Overlay: Translucent mint-white diamond with 50% alpha (indices 30..35)
        add_diamond(-0.5f, 0.5f, rw, rh, {0.85f, 1.00f, 0.90f, 0.50f});

        // 3. Q1 Additive Overlays: Overlapping Coral Red and Emerald Green diamonds (indices 36..47)
        add_diamond(0.5f - 0.08f, 0.5f, rw, rh, {0.85f, 0.10f, 0.20f, 1.0f});
        add_diamond(0.5f + 0.08f, 0.5f, rw, rh, {0.10f, 0.85f, 0.20f, 1.0f});

        // 4. Q2 Multiply Overlay: Electric Cyan filter diamond (indices 48..53)
        add_diamond(-0.5f, -0.5f, rw, rh, {0.00f, 0.85f, 1.00f, 1.0f});

        // 5. Q3 Subtractive Overlay: Crimson Coral diamond (indices 54..59)
        add_diamond(0.5f, -0.5f, rw, rh, {0.80f, 0.15f, 0.00f, 1.0f});

        vbuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vertex_buffer";
            desc.size               = sizeof(BlendVertex) * vertices.size();
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

        auto mapped_v = vbuffer.get_mapped_range<BlendVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(BlendVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint32_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint32_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "blend_modes_shader";
            desc.path   = "blend_modes.slang";
            desc.source = blend_modes_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        auto init_pipe = [&](SimpleRenderPipeline& pipe, const Optional<GPUBlendState>& blend) {
            pipe.vstride = sizeof(BlendVertex);
            pipe.attributes.push_back({"position", offsetof(BlendVertex, position)});
            pipe.attributes.push_back({"color", offsetof(BlendVertex, color)});

            if (blend.has_value()) {
                pipe.init_blend_state(get_backbuffer_format(), blend.value());
            } else {
                pipe.init_color_state(get_backbuffer_format());
            }

            pipe.init_vshader(device, module->get(), "vsmain");
            pipe.init_fshader(device, module->get(), "fsmain");
            pipe.init_playout(device, reflection->get());
            pipe.init_pipeline(device, reflection->get());
        };

        // 1. Opaque pipeline (no blending)
        init_pipe(pipeline_opaque, std::nullopt);

        // 2. Alpha blending: SrcAlpha / (1 - SrcAlpha)
        GPUBlendState alpha_blend{};
        alpha_blend.color.src_factor = GPUBlendFactor::SRC_ALPHA;
        alpha_blend.color.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        alpha_blend.color.operation  = GPUBlendOperation::ADD;
        alpha_blend.alpha.src_factor = GPUBlendFactor::ONE;
        alpha_blend.alpha.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        alpha_blend.alpha.operation  = GPUBlendOperation::ADD;
        init_pipe(pipeline_alpha_blend, alpha_blend);

        // 3. Additive blending: One / One
        GPUBlendState additive_blend{};
        additive_blend.color.src_factor = GPUBlendFactor::ONE;
        additive_blend.color.dst_factor = GPUBlendFactor::ONE;
        additive_blend.color.operation  = GPUBlendOperation::ADD;
        additive_blend.alpha.src_factor = GPUBlendFactor::ONE;
        additive_blend.alpha.dst_factor = GPUBlendFactor::ONE;
        additive_blend.alpha.operation  = GPUBlendOperation::ADD;
        init_pipe(pipeline_additive, additive_blend);

        // 4. Multiply blending: Dst / Zero
        GPUBlendState multiply_blend{};
        multiply_blend.color.src_factor = GPUBlendFactor::DST;
        multiply_blend.color.dst_factor = GPUBlendFactor::ZERO;
        multiply_blend.color.operation  = GPUBlendOperation::ADD;
        multiply_blend.alpha.src_factor = GPUBlendFactor::DST_ALPHA;
        multiply_blend.alpha.dst_factor = GPUBlendFactor::ZERO;
        multiply_blend.alpha.operation  = GPUBlendOperation::ADD;
        init_pipe(pipeline_multiply, multiply_blend);

        // 5. Subtractive blending: Dst - Src (Reverse Subtract)
        GPUBlendState subtract_blend{};
        subtract_blend.color.src_factor = GPUBlendFactor::ONE;
        subtract_blend.color.dst_factor = GPUBlendFactor::ONE;
        subtract_blend.color.operation  = GPUBlendOperation::REVERSE_SUBTRACT;
        subtract_blend.alpha.src_factor = GPUBlendFactor::ZERO;
        subtract_blend.alpha.dst_factor = GPUBlendFactor::ONE;
        subtract_blend.alpha.operation  = GPUBlendOperation::ADD;
        init_pipe(pipeline_subtractive, subtract_blend);
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

        // 1. Draw base cards with pipeline_opaque (30 indices)
        command.set_pipeline(pipeline_opaque.pipeline);
        command.draw_indexed(30, 1, 0, 0, 0);

        // 2. Q0 Alpha Blend diamond (6 indices, offset 30)
        command.set_pipeline(pipeline_alpha_blend.pipeline);
        command.draw_indexed(6, 1, 30, 0, 0);

        // 3. Q1 Additive Overlapping diamonds (12 indices, offset 36)
        command.set_pipeline(pipeline_additive.pipeline);
        command.draw_indexed(12, 1, 36, 0, 0);

        // 4. Q2 Multiply diamond (6 indices, offset 48)
        command.set_pipeline(pipeline_multiply.pipeline);
        command.draw_indexed(6, 1, 48, 0, 0);

        // 5. Q3 Subtractive diamond (6 indices, offset 54)
        command.set_pipeline(pipeline_subtractive.pipeline);
        command.draw_indexed(6, 1, 54, 0, 0);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::raster_blend_modes" * doctest::description("Testing rasterizer color and alpha blend modes."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    BlendModesApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::raster_blend_modes" * doctest::description("Testing rasterizer color and alpha blend modes."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    BlendModesApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::raster_blend_modes" * doctest::description("Testing rasterizer color and alpha blend modes."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    BlendModesApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::raster_blend_modes" * doctest::description("Testing rasterizer color and alpha blend modes."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    BlendModesApp(desc).run();
}
