#include "helper.h"

CString perspective_correction_program = R"""(
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

struct Camera
{
    float4x4 proj;
    float4x4 view;
};

struct Params
{
    ConstantBuffer<Camera> camera;
    Texture2D<float4>      texture;
    SamplerState           sampler;
};

ParameterBlock<Params> params;

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0), params.camera.view), params.camera.proj);
    output.texcoord = input.texcoord;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return params.texture.Sample(params.sampler, input.texcoord);
}
)""";

struct PerspectiveVertex
{
    glm::vec3 position;
    glm::vec2 texcoord;
};

struct PerspectiveCameraUBO
{
    glm::mat4 proj;
    glm::mat4 view;
};

struct PerspectiveCorrectionApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    GPUBuffer            ubuffer;
    GPUBuffer            staging;
    GPUTexture           texture;
    GPUTextureView       texview;
    GPUSampler           sampler;
    GPUBindGroup         bind_group;
    SimpleRenderPipeline pipeline;

    explicit PerspectiveCorrectionApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_texture();
        setup_sampler();
        setup_geometry();
        setup_pipeline();
        setup_descriptors();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        PerspectiveCameraUBO camera{};
        float                fovy   = glm::radians(60.0f);
        float                aspect = float(desc.width) / float(desc.height);
        camera.proj                 = glm::perspective(fovy, aspect, 0.1f, 100.0f);
        camera.view                 = glm::lookAt(glm::vec3(0.0f, 1.8f, 2.5f), glm::vec3(0.0f, -0.5f, -3.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        ubuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "camera_ubuffer";
            bdesc.size               = sizeof(PerspectiveCameraUBO);
            bdesc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        auto mapped  = ubuffer.get_mapped_range<PerspectiveCameraUBO>();
        mapped.at(0) = camera;
    }

    void setup_texture()
    {
        auto& device  = RHI::get_current_device();
        auto& adapter = RHI::get_current_adapter();

        const uint width  = 16;
        const uint height = 16;

        uint row_pitch = align(static_cast<uint>(width * sizeof(uint32_t)), adapter.properties.texture_row_pitch_alignment);
        if (row_pitch == 0) row_pitch = width * sizeof(uint32_t);

        staging = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "checker_staging";
            bdesc.size               = row_pitch * height;
            bdesc.usage              = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        texture = execute([&] {
            auto tdesc            = GPUTextureDescriptor{};
            tdesc.label           = "checker_texture";
            tdesc.size.width      = width;
            tdesc.size.height     = height;
            tdesc.size.depth      = 1;
            tdesc.array_layers    = 1;
            tdesc.dimension       = GPUTextureDimension::x2D;
            tdesc.format          = GPUTextureFormat::RGBA8UNORM;
            tdesc.mip_level_count = 1;
            tdesc.sample_count    = 1;
            tdesc.usage           = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
            return device.create_texture(tdesc);
        });

        texview = texture.create_view();

        auto pack_rgba = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) -> uint32_t {
            return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
                   (static_cast<uint32_t>(a) << 24);
        };

        Vector<uint32_t> pixels(width * height);
        for (uint y = 0; y < height; ++y) {
            for (uint x = 0; x < width; ++x) {
                bool is_white         = ((x ^ y) & 1) != 0;
                pixels[y * width + x] = is_white ? pack_rgba(240, 240, 240) : pack_rgba(20, 20, 25);
            }
        }

        auto* mapped = staging.get_mapped_range<uint8_t>().data;
        for (uint y = 0; y < height; ++y) {
            std::memcpy(mapped + y * row_pitch, &pixels[y * width], width * sizeof(uint32_t));
        }
        staging.unmap();

        auto command = execute([&] {
            auto cdesc  = GPUCommandBufferDescriptor{};
            cdesc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(cdesc);
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

    void setup_sampler()
    {
        auto& device = RHI::get_current_device();

        sampler = execute([&] {
            auto sdesc           = GPUSamplerDescriptor{};
            sdesc.address_mode_u = GPUAddressMode::REPEAT;
            sdesc.address_mode_v = GPUAddressMode::REPEAT;
            sdesc.address_mode_w = GPUAddressMode::REPEAT;
            sdesc.min_filter     = GPUFilterMode::LINEAR;
            sdesc.mag_filter     = GPUFilterMode::LINEAR;
            sdesc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
            sdesc.lod_min_clamp  = 0.0f;
            sdesc.lod_max_clamp  = 1.0f;
            sdesc.max_anisotropy = 1;
            sdesc.label          = "perspective_sampler";
            return device.create_sampler(sdesc);
        });
    }

    void setup_geometry()
    {
        auto& device = RHI::get_current_device();

        // quad on ground plane receding into distance (depth variation from z = 1.0 to z = -7.0)
        PerspectiveVertex vertices[4] = {
            {{-2.0f, -0.5f, 1.0f}, {0.0f, 8.0f}},  // near-left
            {{2.0f, -0.5f, 1.0f}, {8.0f, 8.0f}},   // near-right
            {{2.0f, -0.5f, -7.0f}, {8.0f, 0.0f}},  // far-right
            {{-2.0f, -0.5f, -7.0f}, {0.0f, 0.0f}}, // far-left
        };

        uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

        vbuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "perspective_vbuffer";
            bdesc.size               = sizeof(vertices);
            bdesc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        ibuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "perspective_ibuffer";
            bdesc.size               = sizeof(indices);
            bdesc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        auto mapped_v = vbuffer.get_mapped_range<PerspectiveVertex>();
        std::memcpy(mapped_v.data, vertices, sizeof(vertices));

        auto mapped_i = ibuffer.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i.data, indices, sizeof(indices));
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&] {
            auto cdesc   = CompileDescriptor{};
            cdesc.module = "perspective_correction_shader";
            cdesc.path   = "perspective_correction.slang";
            cdesc.source = perspective_correction_program;
            return compiler->compile(cdesc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(PerspectiveVertex);
        pipeline.attributes.push_back({"position", offsetof(PerspectiveVertex, position)});
        pipeline.attributes.push_back({"texcoord", offsetof(PerspectiveVertex, texcoord)});

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

    void setup_descriptors()
    {
        auto& device = RHI::get_current_device();

        bind_group = execute([&] {
            Array<GPUBindGroupEntry, 3> entries;
            entries[0].type          = GPUResourceType::BUFFER;
            entries[0].binding       = 0;
            entries[0].buffer.buffer = ubuffer;
            entries[0].buffer.offset = 0;
            entries[0].buffer.size   = 0;

            entries[1].type    = GPUResourceType::TEXTURE;
            entries[1].binding = 1;
            entries[1].texture = texview;

            entries[2].type    = GPUResourceType::SAMPLER;
            entries[2].binding = 2;
            entries[2].sampler = sampler;

            auto bdesc    = GPUBindGroupDescriptor{};
            bdesc.heap    = bheap;
            bdesc.layout  = pipeline.blayouts.at(0);
            bdesc.entries = entries;
            return device.create_bind_group(bdesc);
        });
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        auto command = execute([&] {
            auto cdesc  = GPUCommandBufferDescriptor{};
            cdesc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(cdesc);
        });

        auto color_attachment        = GPURenderPassColorAttachment{};
        color_attachment.clear_value = GPUColor{0.08f, 0.09f, 0.12f, 1.0f};
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
        command.set_bind_group(0, bind_group);
        command.draw_indexed(6, 1, 0, 0, 0);

        command.end_render_pass();
        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::texture_perspective_correction" * doctest::description("Testing perspective-correct texture interpolation."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PerspectiveCorrectionApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::texture_perspective_correction" * doctest::description("Testing perspective-correct texture interpolation."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PerspectiveCorrectionApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::texture_perspective_correction" * doctest::description("Testing perspective-correct texture interpolation."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    PerspectiveCorrectionApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::texture_perspective_correction" * doctest::description("Testing perspective-correct texture interpolation."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    PerspectiveCorrectionApp(desc).run();
}
