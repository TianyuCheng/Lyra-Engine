#include "helper.h"

CString texture_lod_program = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float  lod      : TEXCOORD1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float  lod      : TEXCOORD1;
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
    output.lod      = input.lod;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return params.texture.SampleLevel(params.sampler, input.texcoord, input.lod);
}
)""";

struct LODVertex
{
    glm::vec3 position;
    glm::vec2 texcoord;
    float     lod;
};

struct TextureLODApp : public TestApp
{
    GPUBuffer            vbuffer;
    GPUBuffer            ibuffer;
    GPUBuffer            staging;
    GPUTexture           texture;
    GPUTextureView       texview;
    GPUSampler           sampler;
    GPUBindGroup         bind_group;
    SimpleRenderPipeline pipeline;

    explicit TextureLODApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_texture();
        setup_sampler();
        setup_geometry();
        setup_pipeline();
        setup_descriptors();
    }

    void setup_texture()
    {
        auto& device  = RHI::get_current_device();
        auto& adapter = RHI::get_current_adapter();

        constexpr uint mip_count = 6;
        const uint     base_size = 32;

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
            uint w = std::max(1u, base_size >> m);
            uint h = std::max(1u, base_size >> m);
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
            bdesc.label              = "lod_staging";
            bdesc.size               = total_staging_size;
            bdesc.usage              = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        texture = execute([&] {
            auto tdesc            = GPUTextureDescriptor{};
            tdesc.label           = "lod_texture";
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

        const uint32_t mip_colors[mip_count] = {
            pack_rgba(240, 50, 50),   // Mip 0: Red
            pack_rgba(50, 240, 50),   // Mip 1: Green
            pack_rgba(50, 50, 240),   // Mip 2: Blue
            pack_rgba(240, 240, 50),  // Mip 3: Yellow
            pack_rgba(240, 50, 240),  // Mip 4: Magenta
            pack_rgba(50, 240, 240),  // Mip 5: Cyan
        };

        auto* mapped = staging.get_mapped_range<uint8_t>().data;
        for (uint m = 0; m < mip_count; ++m) {
            uint8_t* mip_ptr = mapped + mips[m].offset;
            for (uint y = 0; y < mips[m].height; ++y) {
                uint32_t* row = reinterpret_cast<uint32_t*>(mip_ptr + y * mips[m].row_pitch);
                for (uint x = 0; x < mips[m].width; ++x) {
                    row[x] = mip_colors[m];
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

    void setup_sampler()
    {
        auto& device = RHI::get_current_device();

        sampler = execute([&] {
            auto sdesc           = GPUSamplerDescriptor{};
            sdesc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
            sdesc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
            sdesc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
            sdesc.min_filter     = GPUFilterMode::LINEAR;
            sdesc.mag_filter     = GPUFilterMode::LINEAR;
            sdesc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
            sdesc.lod_min_clamp  = 0.0f;
            sdesc.lod_max_clamp  = 5.0f;
            sdesc.max_anisotropy = 1;
            sdesc.label          = "lod_sampler";
            return device.create_sampler(sdesc);
        });
    }

    void setup_geometry()
    {
        auto& device = RHI::get_current_device();

        Vector<LODVertex> vertices;
        Vector<uint16_t>  indices;

        auto add_quad = [&](float x0, float y0, float x1, float y1, float lod) {
            uint16_t  base = static_cast<uint16_t>(vertices.size());
            LODVertex v00{}, v10{}, v11{}, v01{};
            v00.position = {x0, y0, 0.0f};
            v00.texcoord = {0.0f, 0.0f};
            v00.lod      = lod;

            v10.position = {x1, y0, 0.0f};
            v10.texcoord = {1.0f, 0.0f};
            v10.lod      = lod;

            v11.position = {x1, y1, 0.0f};
            v11.texcoord = {1.0f, 1.0f};
            v11.lod      = lod;

            v01.position = {x0, y1, 0.0f};
            v01.texcoord = {0.0f, 1.0f};
            v01.lod      = lod;

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

        // Top row:
        // Panel 0: Mip 0 (Red)
        add_quad(-0.90f, 0.10f, -0.35f, 0.90f, 0.0f);
        // Panel 1: Mip 1 (Green)
        add_quad(-0.275f, 0.10f, 0.275f, 0.90f, 1.0f);
        // Panel 2: Mip 2 (Blue)
        add_quad(0.35f, 0.10f, 0.90f, 0.90f, 2.0f);

        // Bottom row:
        // Panel 3: Mip 3 (Yellow)
        add_quad(-0.90f, -0.90f, -0.35f, -0.10f, 3.0f);
        // Panel 4: Mip 4 (Magenta)
        add_quad(-0.275f, -0.90f, 0.275f, -0.10f, 4.0f);
        // Panel 5: Fractional LOD 1.5 (Trilinear blend between Green and Blue)
        add_quad(0.35f, -0.90f, 0.90f, -0.10f, 1.5f);

        vbuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "lod_vbuffer";
            bdesc.size               = sizeof(LODVertex) * vertices.size();
            bdesc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        ibuffer = execute([&] {
            auto bdesc               = GPUBufferDescriptor{};
            bdesc.label              = "lod_ibuffer";
            bdesc.size               = sizeof(uint16_t) * indices.size();
            bdesc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            bdesc.mapped_at_creation = true;
            return device.create_buffer(bdesc);
        });

        auto mapped_v = vbuffer.get_mapped_range<LODVertex>();
        std::memcpy(mapped_v.data, vertices.data(), sizeof(LODVertex) * vertices.size());

        auto mapped_i = ibuffer.get_mapped_range<uint16_t>();
        std::memcpy(mapped_i.data, indices.data(), sizeof(uint16_t) * indices.size());
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&] {
            auto cdesc   = CompileDescriptor{};
            cdesc.module = "texture_lod_shader";
            cdesc.path   = "texture_lod.slang";
            cdesc.source = texture_lod_program;
            return compiler->compile(cdesc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(LODVertex);
        pipeline.attributes.push_back({"position", offsetof(LODVertex, position)});
        pipeline.attributes.push_back({"texcoord", offsetof(LODVertex, texcoord)});
        pipeline.attributes.push_back({"lod", offsetof(LODVertex, lod)});

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

        bind_group = execute([&] {
            Array<GPUBindGroupEntry, 2> entries;
            entries[0].type    = GPUResourceType::TEXTURE;
            entries[0].binding = 0;
            entries[0].texture = texview;

            entries[1].type    = GPUResourceType::SAMPLER;
            entries[1].binding = 1;
            entries[1].sampler = sampler;

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
        command.set_bind_group(0, bind_group);
        // 6 quads * 6 indices each = 36 indices
        command.draw_indexed(36, 1, 0, 0, 0);

        command.end_render_pass();
        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::texture_lod" * doctest::description("Testing multi-level texture LOD sampling and trilinear mip filtering."))
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
    TextureLODApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::texture_lod" * doctest::description("Testing multi-level texture LOD sampling and trilinear mip filtering."))
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
    TextureLODApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::texture_lod" * doctest::description("Testing multi-level texture LOD sampling and trilinear mip filtering."))
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
    TextureLODApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::texture_lod" * doctest::description("Testing multi-level texture LOD sampling and trilinear mip filtering."))
{
    TestAppDescriptor desc{};
    desc.name           = "softgpu";
    desc.window         = false;
    desc.backend        = RHIBackend::SOFTGPU;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG;
    desc.compile_target = CompileTarget::HOST;
    desc.compile_flags  = CompileFlag::DEBUG;
    TextureLODApp(desc).run();
}
