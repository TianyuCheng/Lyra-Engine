#include "helper.h"

CString mrt_shader_program = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float4 color0   : COLOR0;
    float4 color1   : COLOR1;
    float4 color2   : COLOR2;
    float4 color3   : COLOR3;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color0   : COLOR0;
    float4 color1   : COLOR1;
    float4 color2   : COLOR2;
    float4 color3   : COLOR3;
};

struct PixelOutput
{
    float4 target0 : SV_Target0;
    float4 target1 : SV_Target1;
    float4 target2 : SV_Target2;
    float4 target3 : SV_Target3;
};

[shader("vertex")]
VertexOutput vs_mrt(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.color0   = input.color0;
    output.color1   = input.color1;
    output.color2   = input.color2;
    output.color3   = input.color3;
    return output;
}

[shader("fragment")]
PixelOutput fs_mrt(VertexOutput input) : SV_Target
{
    PixelOutput output;
    output.target0 = input.color0;
    output.target1 = input.color1;
    output.target2 = input.color2;
    output.target3 = input.color3;
    return output;
}
)""";

CString composite_shader_program = R"""(
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
VertexOutput vs_comp(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texcoord = input.texcoord;
    return output;
}

[shader("fragment")]
float4 fs_comp(VertexOutput input) : SV_Target
{
    return params.texture.Sample(params.sampler, input.texcoord);
}
)""";

struct MRTVertex
{
    glm::vec3 position;
    float     _pad = 0.0f;
    glm::vec4 color0;
    glm::vec4 color1;
    glm::vec4 color2;
    glm::vec4 color3;
};

struct CompVertex
{
    glm::vec3 position;
    glm::vec2 texcoord;
};

struct MultiRenderTargetsApp : public TestApp
{
    GPUBuffer            vbuffer_mrt;
    GPUBuffer            ibuffer_mrt;
    GPUBuffer            vbuffer_comp;
    GPUBuffer            ibuffer_comp;
    GPUTexture           rt[4];
    GPUTextureView       rt_view[4];
    GPUSampler           sampler;
    GPUBindGroup         bg[4];
    SimpleRenderPipeline pipeline_mrt;
    SimpleRenderPipeline pipeline_comp;

    explicit MultiRenderTargetsApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_render_targets();
        setup_geometry();
        setup_pipelines();
        setup_descriptors();
    }

    void setup_render_targets()
    {
        auto& device = RHI::get_current_device();

        for (int i = 0; i < 4; ++i) {
            rt[i] = execute([&] {
                auto desc            = GPUTextureDescriptor{};
                desc.label           = "mrt_target";
                desc.size.width      = desc_app().width;
                desc.size.height     = desc_app().height;
                desc.size.depth      = 1;
                desc.array_layers    = 1;
                desc.dimension       = GPUTextureDimension::x2D;
                desc.format          = GPUTextureFormat::RGBA8UNORM;
                desc.mip_level_count = 1;
                desc.sample_count    = 1;
                desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                return device.create_texture(desc);
            });
            rt_view[i] = rt[i].create_view();
        }

        sampler = execute([&] {
            auto desc           = GPUSamplerDescriptor{};
            desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
            desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
            desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
            desc.min_filter     = GPUFilterMode::LINEAR;
            desc.mag_filter     = GPUFilterMode::LINEAR;
            desc.mipmap_filter  = GPUMipmapFilterMode::NEAREST;
            desc.lod_min_clamp  = 0.0f;
            desc.lod_max_clamp  = 1.0f;
            desc.max_anisotropy = 1;
            return device.create_sampler(desc);
        });
    }

    const TestAppDescriptor& desc_app() const { return desc; }

    void setup_geometry()
    {
        auto& device = RHI::get_current_device();

        // 1. MRT Scene: Center Slate Card + Center Diamond Emblem
        Vector<MRTVertex> mrt_verts;
        Vector<uint16_t>  mrt_indices;

        const glm::vec4 card_color{0.10f, 0.12f, 0.16f, 1.0f};
        const glm::vec4 ruby{1.00f, 0.15f, 0.25f, 1.0f};
        const glm::vec4 emerald{0.15f, 1.00f, 0.35f, 1.0f};
        const glm::vec4 sapphire{0.20f, 0.45f, 1.00f, 1.0f};
        const glm::vec4 amber{1.00f, 0.80f, 0.15f, 1.0f};

        auto add_card_vert = [&](float x, float y) {
            MRTVertex v{};
            v.position = {x, y, 0.0f};
            v.color0   = card_color;
            v.color1   = card_color;
            v.color2   = card_color;
            v.color3   = card_color;
            mrt_verts.push_back(v);
        };

        // Card: -0.65 to 0.65
        add_card_vert(-0.65f, -0.65f);
        add_card_vert(0.65f, -0.65f);
        add_card_vert(0.65f, 0.65f);
        add_card_vert(-0.65f, 0.65f);
        mrt_indices.push_back(0);
        mrt_indices.push_back(1);
        mrt_indices.push_back(2);
        mrt_indices.push_back(0);
        mrt_indices.push_back(2);
        mrt_indices.push_back(3);

        auto add_emblem_vert = [&](float x, float y) {
            MRTVertex v{};
            v.position = {x, y, 0.0f};
            v.color0   = ruby;
            v.color1   = emerald;
            v.color2   = sapphire;
            v.color3   = amber;
            mrt_verts.push_back(v);
        };

        // Diamond emblem:
        add_emblem_vert(0.0f, 0.42f);  // top
        add_emblem_vert(-0.42f, 0.0f); // left
        add_emblem_vert(0.0f, -0.42f); // bottom
        add_emblem_vert(0.42f, 0.0f);  // right
        mrt_indices.push_back(4);
        mrt_indices.push_back(5);
        mrt_indices.push_back(6);
        mrt_indices.push_back(4);
        mrt_indices.push_back(6);
        mrt_indices.push_back(7);

        vbuffer_mrt = execute([&] {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vbuffer_mrt";
            desc.size               = sizeof(MRTVertex) * mrt_verts.size();
            desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        ibuffer_mrt = execute([&] {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "ibuffer_mrt";
            desc.size               = sizeof(uint16_t) * mrt_indices.size();
            desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        std::memcpy(vbuffer_mrt.get_mapped_range<MRTVertex>().data, mrt_verts.data(), sizeof(MRTVertex) * mrt_verts.size());
        std::memcpy(ibuffer_mrt.get_mapped_range<uint16_t>().data, mrt_indices.data(), sizeof(uint16_t) * mrt_indices.size());

        // 2. Composite Geometry: 4 Quadrants
        Vector<CompVertex> comp_verts;
        Vector<uint16_t>   comp_indices;

        auto add_tile = [&](float x0, float y0, float x1, float y1) {
            uint16_t base = static_cast<uint16_t>(comp_verts.size());
            comp_verts.push_back({{x0, y0, 0.0f}, {0.0f, 1.0f}});
            comp_verts.push_back({{x1, y0, 0.0f}, {1.0f, 1.0f}});
            comp_verts.push_back({{x1, y1, 0.0f}, {1.0f, 0.0f}});
            comp_verts.push_back({{x0, y1, 0.0f}, {0.0f, 0.0f}});

            comp_indices.push_back(base + 0);
            comp_indices.push_back(base + 1);
            comp_indices.push_back(base + 2);
            comp_indices.push_back(base + 0);
            comp_indices.push_back(base + 2);
            comp_indices.push_back(base + 3);
        };

        const float hw = 0.44f;
        const float hh = 0.44f;
        // Q0: Top-Left
        add_tile(-0.5f - hw, 0.5f - hh, -0.5f + hw, 0.5f + hh);
        // Q1: Top-Right
        add_tile(0.5f - hw, 0.5f - hh, 0.5f + hw, 0.5f + hh);
        // Q2: Bottom-Left
        add_tile(-0.5f - hw, -0.5f - hh, -0.5f + hw, -0.5f + hh);
        // Q3: Bottom-Right
        add_tile(0.5f - hw, -0.5f - hh, 0.5f + hw, -0.5f + hh);

        vbuffer_comp = execute([&] {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "vbuffer_comp";
            desc.size               = sizeof(CompVertex) * comp_verts.size();
            desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        ibuffer_comp = execute([&] {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "ibuffer_comp";
            desc.size               = sizeof(uint16_t) * comp_indices.size();
            desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        std::memcpy(vbuffer_comp.get_mapped_range<CompVertex>().data, comp_verts.data(), sizeof(CompVertex) * comp_verts.size());
        std::memcpy(ibuffer_comp.get_mapped_range<uint16_t>().data, comp_indices.data(), sizeof(uint16_t) * comp_indices.size());
    }

    void setup_pipelines()
    {
        auto& device = RHI::get_current_device();

        // Pipeline 1: MRT Pipeline
        auto module_mrt = execute([&] {
            auto desc   = CompileDescriptor{};
            desc.module = "mrt_shader";
            desc.path   = "mrt.slang";
            desc.source = mrt_shader_program;
            return compiler->compile(desc);
        });

        auto refl_mrt = compiler->reflect({
            {*module_mrt, "vs_mrt"},
            {*module_mrt, "fs_mrt"},
        });

        pipeline_mrt.vstride = sizeof(MRTVertex);
        pipeline_mrt.attributes.push_back({"position", offsetof(MRTVertex, position)});
        pipeline_mrt.attributes.push_back({"color0", offsetof(MRTVertex, color0)});
        pipeline_mrt.attributes.push_back({"color1", offsetof(MRTVertex, color1)});
        pipeline_mrt.attributes.push_back({"color2", offsetof(MRTVertex, color2)});
        pipeline_mrt.attributes.push_back({"color3", offsetof(MRTVertex, color3)});

        for (int i = 0; i < 4; ++i) {
            pipeline_mrt.init_color_state(GPUTextureFormat::RGBA8UNORM);
        }
        pipeline_mrt.init_vshader(device, module_mrt.get(), "vs_mrt");
        pipeline_mrt.init_fshader(device, module_mrt.get(), "fs_mrt");
        pipeline_mrt.init_playout(device, refl_mrt.get());
        pipeline_mrt.init_pipeline(device, refl_mrt.get());

        // Pipeline 2: Composite Pipeline
        auto module_comp = execute([&] {
            auto desc   = CompileDescriptor{};
            desc.module = "comp_shader";
            desc.path   = "comp.slang";
            desc.source = composite_shader_program;
            return compiler->compile(desc);
        });

        auto refl_comp = compiler->reflect({
            {*module_comp, "vs_comp"},
            {*module_comp, "fs_comp"},
        });

        pipeline_comp.vstride = sizeof(CompVertex);
        pipeline_comp.attributes.push_back({"position", offsetof(CompVertex, position)});
        pipeline_comp.attributes.push_back({"texcoord", offsetof(CompVertex, texcoord)});

        pipeline_comp.init_color_state(get_backbuffer_format());
        pipeline_comp.init_vshader(device, module_comp.get(), "vs_comp");
        pipeline_comp.init_fshader(device, module_comp.get(), "fs_comp");
        pipeline_comp.init_playout(device, refl_comp.get());
        pipeline_comp.init_pipeline(device, refl_comp.get());
    }

    void setup_descriptors()
    {
        auto& device = RHI::get_current_device();

        for (int i = 0; i < 4; ++i) {
            bg[i] = execute([&] {
                Array<GPUBindGroupEntry, 2> entries;
                entries[0].type    = GPUResourceType::TEXTURE;
                entries[0].binding = 0;
                entries[0].texture = rt_view[i];

                entries[1].type    = GPUResourceType::SAMPLER;
                entries[1].binding = 1;
                entries[1].sampler = sampler;

                auto desc    = GPUBindGroupDescriptor{};
                desc.heap    = bheap;
                desc.layout  = pipeline_comp.blayouts.at(0);
                desc.entries = entries;
                return device.create_bind_group(desc);
            });
        }
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        auto command = execute([&] {
            auto desc  = GPUCommandBufferDescriptor{};
            desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        // ==========================================
        // PASS 1: MRT Render Pass
        // ==========================================
        for (int i = 0; i < 4; ++i) {
            command.resource_barrier(state_transition(rt[i], undefined_texture_state(), color_attachment_texture_state()));
        }

        Array<GPURenderPassColorAttachment, 4> mrt_attachments;
        for (int i = 0; i < 4; ++i) {
            mrt_attachments[i].clear_value = GPUColor{0.063f, 0.078f, 0.102f, 1.0f}; // #10141A
            mrt_attachments[i].load_op     = GPULoadOp::CLEAR;
            mrt_attachments[i].store_op    = GPUStoreOp::STORE;
            mrt_attachments[i].view        = rt_view[i];
        }

        auto mrt_pass              = GPURenderPassDescriptor{};
        mrt_pass.color_attachments = mrt_attachments;

        command.begin_render_pass(mrt_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);

        command.set_pipeline(pipeline_mrt.pipeline);
        command.set_vertex_buffer(0, vbuffer_mrt);
        command.set_index_buffer(ibuffer_mrt, GPUIndexFormat::UINT16, 0);
        command.draw_indexed(12, 1, 0, 0, 0); // Card (6) + Diamond (6)
        command.end_render_pass();

        // Transition MRT textures from color attachment to shader resource
        for (int i = 0; i < 4; ++i) {
            command.resource_barrier(state_transition(rt[i], color_attachment_texture_state(), shader_resource_texture_state(GPUBarrierSync::PIXEL_SHADING)));
        }

        // ==========================================
        // PASS 2: Composite Pass to Backbuffer
        // ==========================================
        command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), color_attachment_texture_state()));

        auto comp_attachment        = GPURenderPassColorAttachment{};
        comp_attachment.clear_value = GPUColor{0.035f, 0.043f, 0.055f, 1.0f};
        comp_attachment.load_op     = GPULoadOp::CLEAR;
        comp_attachment.store_op    = GPUStoreOp::STORE;
        comp_attachment.view        = backbuffer.view;

        auto comp_pass              = GPURenderPassDescriptor{};
        comp_pass.color_attachments = comp_attachment;

        command.begin_render_pass(comp_pass);
        command.set_viewport(0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height));
        command.set_scissor_rect(0, 0, desc.width, desc.height);

        command.set_pipeline(pipeline_comp.pipeline);
        command.set_vertex_buffer(0, vbuffer_comp);
        command.set_index_buffer(ibuffer_comp, GPUIndexFormat::UINT16, 0);

        for (int i = 0; i < 4; ++i) {
            command.set_bind_group(0, bg[i]);
            command.draw_indexed(6, 1, i * 6, 0, 0);
        }

        command.end_render_pass();

        postprocessing(command, backbuffer.texture);
        command.submit();
    }
};

#if defined(LYRA_VULKAN_SUPPORT)
TEST_CASE("rhi::vulkan::multi_render_targets" * doctest::description("Testing multiple render targets (MRT) and simultaneous attachment writes."))
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
    MultiRenderTargetsApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::multi_render_targets" * doctest::description("Testing multiple render targets (MRT) and simultaneous attachment writes."))
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
    MultiRenderTargetsApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::multi_render_targets" * doctest::description("Testing multiple render targets (MRT) and simultaneous attachment writes."))
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
    MultiRenderTargetsApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::multi_render_targets" * doctest::description("Testing multiple render targets (MRT) and simultaneous attachment writes."))
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
    MultiRenderTargetsApp(desc).run();
}
