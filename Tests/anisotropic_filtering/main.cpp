#include "helper.h"

CString anisotropic_filtering_program = R"""(
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

struct AnisoVertex
{
    glm::vec3 position;
    glm::vec2 texcoord;
};

struct AnisoCameraUBO
{
    glm::mat4 proj;
    glm::mat4 view;
};

struct AnisotropicFilteringApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    GPUBuffer            ubuffer;
    GPUBuffer            staging;
    GPUTexture           texture;
    GPUTextureView       texview;
    GPUSampler           sampler_iso;
    GPUSampler           sampler_aniso;
    GPUBindGroup         bg_iso;
    GPUBindGroup         bg_aniso;
    SimpleRenderPipeline pipeline;

    explicit AnisotropicFilteringApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_texture();
        setup_samplers();
        setup_geometry();
        setup_pipeline();
        setup_descriptors();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        AnisoCameraUBO camera{};
        float          fovy   = glm::radians(55.0f);
        float          aspect = float(desc.width) / float(desc.height);
        camera.proj           = glm::perspective(fovy, aspect, 0.1f, 100.0f);
        camera.view           = glm::lookAt(glm::vec3(0.0f, 0.8f, 2.0f), glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        ubuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "aniso_camera_ubuffer";
            bdesc.size               = sizeof(AnisoCameraUBO);
            bdesc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        auto mapped  = ubuffer.get_mapped_range<AnisoCameraUBO>();
        mapped.at(0) = camera;
    }

    void setup_texture()
    {
        auto& device  = RHI::get_current_device();
        auto& adapter = RHI::get_current_adapter();

        constexpr uint mip_count = 7;
        const uint     base_size = 64;

        struct MipInfo
        {
            uint width;
            uint height;
            uint row_pitch;
            uint offset;
        };

        MipInfo mips[mip_count];
        uint    total_staging_size = 0;

        for (uint m = 0; m < mip_count; ++m) {
            uint w     = std::max(1u, base_size >> m);
            uint h     = std::max(1u, base_size >> m);
            uint pitch = align(static_cast<uint>(w * sizeof(uint32_t)), adapter.properties.texture_row_pitch_alignment);
            if (pitch == 0) pitch = w * sizeof(uint32_t);

            mips[m].width     = w;
            mips[m].height    = h;
            mips[m].row_pitch = pitch;
            mips[m].offset    = total_staging_size;

            total_staging_size += pitch * h;
        }

        staging = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "aniso_staging";
            bdesc.size               = total_staging_size;
            bdesc.usage              = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        texture = execute([&] {
            auto tdesc            = GPUTextureDescriptor{};
            tdesc.label           = "aniso_texture";
            tdesc.size.width      = base_size;
            tdesc.size.height     = base_size;
            tdesc.size.depth      = 1;
            tdesc.array_layers    = 1;
            tdesc.dimension       = GPUTextureDimension::x2D;
            tdesc.format          = GPUTextureFormat::RGBA8UNORM;
            tdesc.mip_level_count = mip_count;
            tdesc.sample_count    = 1;
            tdesc.usage           = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
            return device.create_texture(tdesc);
        });

        texview = texture.create_view();

        auto pack_rgba = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) -> uint32_t {
            return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
                   (static_cast<uint32_t>(a) << 24);
        };

        // generate mip 0 high-frequency grid pattern
        Vector<Vector<uint32_t>> mip_pixels(mip_count);
        mip_pixels[0].resize(base_size * base_size);
        for (uint y = 0; y < base_size; ++y) {
            for (uint x = 0; x < base_size; ++x) {
                bool stripe                      = (((x / 2) ^ (y / 2)) & 1) != 0;
                mip_pixels[0][y * base_size + x] = stripe ? pack_rgba(255, 255, 255) : pack_rgba(10, 10, 15);
            }
        }

        // generate lower mips via 2x2 box filtering
        for (uint m = 1; m < mip_count; ++m) {
            uint pw = mips[m - 1].width;
            uint w  = mips[m].width;
            uint h  = mips[m].height;
            mip_pixels[m].resize(w * h);

            for (uint y = 0; y < h; ++y) {
                for (uint x = 0; x < w; ++x) {
                    uint32_t c00 = mip_pixels[m - 1][(2 * y + 0) * pw + (2 * x + 0)];
                    uint32_t c10 = mip_pixels[m - 1][(2 * y + 0) * pw + std::min(2 * x + 1, pw - 1)];
                    uint32_t c01 = mip_pixels[m - 1][std::min(2 * y + 1, mips[m - 1].height - 1) * pw + (2 * x + 0)];
                    uint32_t c11 = mip_pixels[m - 1][std::min(2 * y + 1, mips[m - 1].height - 1) * pw + std::min(2 * x + 1, pw - 1)];

                    auto get_ch = [](uint32_t c, int s) -> uint32_t {
                        return (c >> s) & 0xFFu;
                    };
                    uint32_t r = (get_ch(c00, 0) + get_ch(c10, 0) + get_ch(c01, 0) + get_ch(c11, 0)) / 4;
                    uint32_t g = (get_ch(c00, 8) + get_ch(c10, 8) + get_ch(c01, 8) + get_ch(c11, 8)) / 4;
                    uint32_t b = (get_ch(c00, 16) + get_ch(c10, 16) + get_ch(c01, 16) + get_ch(c11, 16)) / 4;
                    uint32_t a = (get_ch(c00, 24) + get_ch(c10, 24) + get_ch(c01, 24) + get_ch(c11, 24)) / 4;

                    mip_pixels[m][y * w + x] = pack_rgba(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(a));
                }
            }
        }

        auto* mapped = staging.get_mapped_range<uint8_t>().data;
        for (uint m = 0; m < mip_count; ++m) {
            uint8_t* mip_ptr = mapped + mips[m].offset;
            for (uint y = 0; y < mips[m].height; ++y) {
                uint32_t* row = reinterpret_cast<uint32_t*>(mip_ptr + y * mips[m].row_pitch);
                for (uint x = 0; x < mips[m].width; ++x) {
                    row[x] = mip_pixels[m][y * mips[m].width + x];
                }
            }
        }
        staging.unmap();

        auto command = execute([&] {
            auto cdesc  = GPUCommandBufferDescriptor{};
            cdesc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(cdesc);
        });

        command.resource_barrier(state_transition(texture, undefined_texture_state(), copy_dst_texture_state(), 0, 1, 0, mip_count));

        for (uint m = 0; m < mip_count; ++m) {
            auto copy_src           = GPUTexelCopyBufferInfo{};
            copy_src.buffer         = staging;
            copy_src.bytes_per_row  = mips[m].row_pitch;
            copy_src.rows_per_image = mips[m].height;
            copy_src.offset         = mips[m].offset;

            auto copy_dst      = GPUTexelCopyTextureInfo{};
            copy_dst.texture   = texture;
            copy_dst.aspect    = GPUTextureAspect::COLOR;
            copy_dst.mip_level = m;
            copy_dst.origin    = {0, 0, 0};

            auto copy_ext   = GPUExtent3D{};
            copy_ext.width  = mips[m].width;
            copy_ext.height = mips[m].height;
            copy_ext.depth  = 1;

            command.copy_buffer_to_texture(copy_src, copy_dst, copy_ext);
        }

        command.resource_barrier(state_transition(texture, copy_dst_texture_state(), shader_resource_texture_state(GPUBarrierSync::PIXEL_SHADING), 0, 1, 0, mip_count));
        command.submit();

        device.wait();
    }

    void setup_samplers()
    {
        auto& device = RHI::get_current_device();

        auto make_sampler = [&](uint anisotropy, CString label) {
            auto sdesc           = GPUSamplerDescriptor{};
            sdesc.address_mode_u = GPUAddressMode::REPEAT;
            sdesc.address_mode_v = GPUAddressMode::REPEAT;
            sdesc.address_mode_w = GPUAddressMode::REPEAT;
            sdesc.min_filter     = GPUFilterMode::LINEAR;
            sdesc.mag_filter     = GPUFilterMode::LINEAR;
            sdesc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
            sdesc.lod_min_clamp  = 0.0f;
            sdesc.lod_max_clamp  = 7.0f;
            sdesc.max_anisotropy = anisotropy;
            sdesc.label          = label;
            return device.create_sampler(sdesc);
        };

        sampler_iso   = make_sampler(1, "sampler_isotropic");
        sampler_aniso = make_sampler(16, "sampler_anisotropic");
    }

    void setup_geometry()
    {
        auto& device = RHI::get_current_device();

        // two side-by-side quads on ground plane y = 0, receding into distance
        AnisoVertex vertices[8] = {
            // left quad (isotropic)
            {{-2.2f, 0.0f, 1.0f}, {0.0f, 16.0f}},
            {{-0.1f, 0.0f, 1.0f}, {8.0f, 16.0f}},
            {{-0.1f, 0.0f, -12.0f}, {8.0f, 0.0f}},
            {{-2.2f, 0.0f, -12.0f}, {0.0f, 0.0f}},

            // right quad (anisotropic)
            {{0.1f, 0.0f, 1.0f}, {0.0f, 16.0f}},
            {{2.2f, 0.0f, 1.0f}, {8.0f, 16.0f}},
            {{2.2f, 0.0f, -12.0f}, {8.0f, 0.0f}},
            {{0.1f, 0.0f, -12.0f}, {0.0f, 0.0f}},
        };

        uint16_t indices[12] = {
            // left quad
            0,
            1,
            2,
            0,
            2,
            3,
            // right quad
            4,
            5,
            6,
            4,
            6,
            7,
        };

        vbuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "aniso_vbuffer";
            bdesc.size               = sizeof(vertices);
            bdesc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        ibuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "aniso_ibuffer";
            bdesc.size               = sizeof(indices);
            bdesc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        auto mapped_v = vbuffer.get_mapped_range<AnisoVertex>();
        std::memcpy(mapped_v.data, vertices, sizeof(vertices));

        auto mapped_i = ibuffer.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i.data, indices, sizeof(indices));
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&] {
            auto cdesc   = CompileDescriptor{};
            cdesc.module = "anisotropic_filtering_shader";
            cdesc.path   = "anisotropic_filtering.slang";
            cdesc.source = anisotropic_filtering_program;
            return compiler->compile(cdesc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(AnisoVertex);
        pipeline.attributes.push_back({"position", offsetof(AnisoVertex, position)});
        pipeline.attributes.push_back({"texcoord", offsetof(AnisoVertex, texcoord)});

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
        };

        bg_iso   = create_bg(sampler_iso);
        bg_aniso = create_bg(sampler_aniso);
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

        // draw left quad with isotropic sampler
        command.set_bind_group(0, bg_iso);
        command.draw_indexed(6, 1, 0, 0, 0);

        // draw right quad with anisotropic sampler
        command.set_bind_group(0, bg_aniso);
        command.draw_indexed(6, 1, 6, 0, 0);

        command.end_render_pass();
        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::anisotropic_filtering" * doctest::description("Testing anisotropic sampler filtering (1x vs 16x)."))
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
    AnisotropicFilteringApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::anisotropic_filtering" * doctest::description("Testing anisotropic sampler filtering (1x vs 16x)."))
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
    AnisotropicFilteringApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::anisotropic_filtering" * doctest::description("Testing anisotropic sampler filtering (1x vs 16x)."))
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
    AnisotropicFilteringApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::anisotropic_filtering" * doctest::description("Testing anisotropic sampler filtering (1x vs 16x)."))
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
    AnisotropicFilteringApp(desc).run();
}
