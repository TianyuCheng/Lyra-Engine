#include "helper.h"

CString sampler_modes_program = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

struct Params
{
    Texture2D<float4> texture;
    SamplerState      sampler;
};

ParameterBlock<Params> params;

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texcoord = input.texcoord;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return params.texture.Sample(params.sampler, input.texcoord);
}
)""";

struct SamplerVertex
{
    glm::vec3 position;
    glm::vec2 texcoord;
};

struct SamplerModesApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    GPUBuffer            staging;
    GPUTexture           texture;
    GPUTextureView       texview;
    GPUSampler           sampler_clamp;
    GPUSampler           sampler_repeat;
    GPUSampler           sampler_mirror;
    GPUSampler           sampler_linear;
    GPUBindGroup         bg_clamp;
    GPUBindGroup         bg_repeat;
    GPUBindGroup         bg_mirror;
    GPUBindGroup         bg_linear;
    SimpleRenderPipeline pipeline;

    explicit SamplerModesApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_texture();
        setup_samplers();
        setup_geometry();
        setup_pipeline();
        setup_descriptors();
    }

    void setup_texture()
    {
        auto& device  = RHI::get_current_device();
        auto& adapter = RHI::get_current_adapter();

        const uint width  = 4;
        const uint height = 4;

        uint row_pitch = align(static_cast<uint>(width * sizeof(uint32_t)), adapter.properties.texture_row_pitch_alignment);
        if (row_pitch == 0) row_pitch = width * sizeof(uint32_t);

        staging = execute([&] {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "texture_staging";
            desc.size               = row_pitch * height;
            desc.usage              = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        texture = execute([&] {
            auto desc            = GPUTextureDescriptor{};
            desc.label           = "pattern_texture";
            desc.size.width      = width;
            desc.size.height     = height;
            desc.size.depth      = 1;
            desc.array_layers    = 1;
            desc.dimension       = GPUTextureDimension::x2D;
            desc.format          = GPUTextureFormat::RGBA8UNORM;
            desc.mip_level_count = 1;
            desc.sample_count    = 1;
            desc.usage           = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
            return device.create_texture(desc);
        });

        texview = texture.create_view();

        auto pack_rgba = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) -> uint32_t {
            return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
                   (static_cast<uint32_t>(a) << 24);
        };

        const uint32_t pixels[4][4] = {
            {pack_rgba(255, 38, 64), pack_rgba(255, 140, 26), pack_rgba(255, 223, 26), pack_rgba(255, 255, 255)},
            {pack_rgba(230, 26, 230), pack_rgba(255, 87, 51), pack_rgba(128, 255, 26), pack_rgba(26, 230, 255)},
            {pack_rgba(140, 26, 230), pack_rgba(26, 83, 255), pack_rgba(26, 255, 223), pack_rgba(128, 255, 179)},
            {pack_rgba(16, 32, 96), pack_rgba(32, 16, 128), pack_rgba(16, 96, 32), pack_rgba(32, 32, 32)},
        };

        auto* mapped = staging.get_mapped_range<uint8_t>().data;
        for (uint y = 0; y < height; ++y) {
            std::memcpy(mapped + y * row_pitch, pixels[y], width * sizeof(uint32_t));
        }
        staging.unmap();

        auto command = execute([&] {
            auto desc  = GPUCommandBufferDescriptor{};
            desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        auto copy_src           = GPUTexelCopyBufferInfo{};
        copy_src.buffer         = staging;
        copy_src.bytes_per_row  = row_pitch;
        copy_src.rows_per_image = height;
        copy_src.offset         = 0;

        auto copy_dst      = GPUTexelCopyTextureInfo{};
        copy_dst.texture   = texture;
        copy_dst.aspect    = GPUTextureAspect::COLOR;
        copy_dst.mip_level = 0;
        copy_dst.origin    = {0, 0, 0};

        auto copy_ext   = GPUExtent3D{};
        copy_ext.width  = width;
        copy_ext.height = height;
        copy_ext.depth  = 1;

        command.resource_barrier(state_transition(texture, undefined_texture_state(), copy_dst_texture_state()));
        command.copy_buffer_to_texture(copy_src, copy_dst, copy_ext);
        command.resource_barrier(state_transition(texture, copy_dst_texture_state(), shader_resource_texture_state(GPUBarrierSync::PIXEL_SHADING)));
        command.submit();

        device.wait();
    }

    void setup_samplers()
    {
        auto& device = RHI::get_current_device();

        auto make_sampler = [&](GPUAddressMode address_mode, GPUFilterMode filter) {
            auto desc           = GPUSamplerDescriptor{};
            desc.address_mode_u = address_mode;
            desc.address_mode_v = address_mode;
            desc.address_mode_w = address_mode;
            desc.min_filter     = filter;
            desc.mag_filter     = filter;
            desc.mipmap_filter  = GPUMipmapFilterMode::NEAREST;
            desc.lod_min_clamp  = 0.0f;
            desc.lod_max_clamp  = 1.0f;
            desc.max_anisotropy = 1;
            return device.create_sampler(desc);
        };

        sampler_clamp  = make_sampler(GPUAddressMode::CLAMP_TO_EDGE, GPUFilterMode::NEAREST);
        sampler_repeat = make_sampler(GPUAddressMode::REPEAT, GPUFilterMode::NEAREST);
        sampler_mirror = make_sampler(GPUAddressMode::MIRROR_REPEAT, GPUFilterMode::NEAREST);
        sampler_linear = make_sampler(GPUAddressMode::REPEAT, GPUFilterMode::LINEAR);
    }

    void setup_geometry()
    {
        auto& device = RHI::get_current_device();

        Vector<SamplerVertex> vertices;
        Vector<uint16_t>      indices;

        auto add_quad = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
            uint16_t      base = static_cast<uint16_t>(vertices.size());
            SamplerVertex v00{}, v10{}, v11{}, v01{};
            v00.position = {x0, y0, 0.0f};
            v00.texcoord = {u0, v0};
            v10.position = {x1, y0, 0.0f};
            v10.texcoord = {u1, v0};
            v11.position = {x1, y1, 0.0f};
            v11.texcoord = {u1, v1};
            v01.position = {x0, y1, 0.0f};
            v01.texcoord = {u0, v1};

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

        // Q0: Top-Left (UV 0.0 to 2.0)
        add_quad(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh, 0.0f, 0.0f, 2.0f, 2.0f);
        // Q1: Top-Right (UV 0.0 to 2.0)
        add_quad(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh, 0.0f, 0.0f, 2.0f, 2.0f);
        // Q2: Bottom-Left (UV 0.0 to 2.0)
        add_quad(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh, 0.0f, 0.0f, 2.0f, 2.0f);
        // Q3: Bottom-Right (UV 0.0 to 2.0)
        add_quad(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh, 0.0f, 0.0f, 2.0f, 2.0f);

        vbuffer = execute([&] {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "sampler_vbuffer";
            desc.size               = sizeof(SamplerVertex) * vertices.size();
            desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        ibuffer = execute([&] {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "sampler_ibuffer";
            desc.size               = sizeof(uint16_t) * indices.size();
            desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        auto mapped_v = vbuffer.get_mapped_range<SamplerVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(SamplerVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint16_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&] {
            auto desc   = CompileDescriptor{};
            desc.module = "sampler_modes_shader";
            desc.path   = "sampler_modes.slang";
            desc.source = sampler_modes_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(SamplerVertex);
        pipeline.attributes.push_back({"position", offsetof(SamplerVertex, position)});
        pipeline.attributes.push_back({"texcoord", offsetof(SamplerVertex, texcoord)});

        GPUPrimitiveState prim{};
        prim.topology      = GPUPrimitiveTopology::TRIANGLE_LIST;
        prim.cull_mode     = GPUCullMode::NONE;
        pipeline.primitive = prim;

        pipeline.init_color_state(get_backbuffer_format());
        pipeline.init_vshader(device, module.get(), "vsmain");
        pipeline.init_fshader(device, module.get(), "fsmain");
        pipeline.init_playout(device, reflection.get());
        pipeline.init_pipeline(device, reflection.get());
    }

    void setup_descriptors()
    {
        auto& device = RHI::get_current_device();

        auto create_bg = [&](const GPUSampler& sampler) {
            Array<GPUBindGroupEntry, 2> entries;
            entries[0].type    = GPUResourceType::TEXTURE;
            entries[0].binding = 0;
            entries[0].texture = texview;

            entries[1].type    = GPUResourceType::SAMPLER;
            entries[1].binding = 1;
            entries[1].sampler = sampler;

            auto desc    = GPUBindGroupDescriptor{};
            desc.heap    = bheap;
            desc.layout  = pipeline.blayouts.at(0);
            desc.entries = entries;
            return device.create_bind_group(desc);
        };

        bg_clamp  = create_bg(sampler_clamp);
        bg_repeat = create_bg(sampler_repeat);
        bg_mirror = create_bg(sampler_mirror);
        bg_linear = create_bg(sampler_linear);
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        auto command = execute([&] {
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

        command.set_pipeline(pipeline.pipeline);
        command.set_vertex_buffer(0, vbuffer);
        command.set_index_buffer(ibuffer, GPUIndexFormat::UINT16, 0);

        // Q0: Top-Left (CLAMP_TO_EDGE + NEAREST)
        command.set_bind_group(0, bg_clamp);
        command.draw_indexed(6, 1, 0, 0, 0);

        // Q1: Top-Right (REPEAT + NEAREST)
        command.set_bind_group(0, bg_repeat);
        command.draw_indexed(6, 1, 6, 0, 0);

        // Q2: Bottom-Left (MIRROR_REPEAT + NEAREST)
        command.set_bind_group(0, bg_mirror);
        command.draw_indexed(6, 1, 12, 0, 0);

        // Q3: Bottom-Right (REPEAT + LINEAR)
        command.set_bind_group(0, bg_linear);
        command.draw_indexed(6, 1, 18, 0, 0);

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#if defined(LYRA_VULKAN_SUPPORT)
TEST_CASE("rhi::vulkan::texture_sampler_modes" * doctest::description("Testing sampler address modes (CLAMP, REPEAT, MIRROR) and filtering (NEAREST, LINEAR)."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    SamplerModesApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::texture_sampler_modes" * doctest::description("Testing sampler address modes (CLAMP, REPEAT, MIRROR) and filtering (NEAREST, LINEAR)."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    SamplerModesApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::texture_sampler_modes" * doctest::description("Testing sampler address modes (CLAMP, REPEAT, MIRROR) and filtering (NEAREST, LINEAR)."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    SamplerModesApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::texture_sampler_modes" * doctest::description("Testing sampler address modes (CLAMP, REPEAT, MIRROR) and filtering (NEAREST, LINEAR)."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    SamplerModesApp(desc).run();
}
