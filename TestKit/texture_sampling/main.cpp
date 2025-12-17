#include "helper.h"

CString texture_sampling_program = R"""(
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

struct TextureSamplingApp : public TestApp
{
    Uniform              uniform;
    Geometry             geometry;
    GPUBuffer            staging;
    GPUTexture           texture;
    GPUTextureView       texview;
    GPUSampler           sampler;
    GPUBindGroup         bind_group;
    SimpleRenderPipeline pipeline;

    explicit TextureSamplingApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_sampler();
        setup_texture();
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

    void setup_texture()
    {
        auto tex = SimpleTexture2D::gradient(4, 4);
        tex.upload();

        staging = tex.buffer;
        texture = tex.texture;
        texview = tex.view;
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "test";
            desc.path   = "test.slang";
            desc.source = texture_sampling_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "vsmain"},
            {*module, "fsmain"},
        });

        pipeline.vstride = sizeof(Vertex);
        pipeline.attributes.push_back({"position", offsetof(Vertex, position)});
        pipeline.attributes.push_back({"texcoord", offsetof(Vertex, uv)});
        pipeline.init_color_state(get_backbuffer_format());
        pipeline.init_vshader(device, module.get(), "vsmain");
        pipeline.init_fshader(device, module.get(), "fsmain");
        pipeline.init_playout(device, reflection.get());
        pipeline.init_pipeline(device, reflection.get());
    }

    void setup_descriptors()
    {
        auto device = RHI::get_current_device();

        // create bind group
        bind_group = execute([&]() {
            Array<GPUBindGroupEntry, 3> entries;

            // camera
            {
                auto& entry         = entries.at(0);
                entry.type          = GPUBindingResourceType::BUFFER;
                entry.binding       = 0;
                entry.buffer.buffer = uniform.ubuffer;
                entry.buffer.offset = 0;
                entry.buffer.size   = 0;
            }

            // texture
            {
                auto& entry   = entries.at(1);
                entry.type    = GPUBindingResourceType::TEXTURE;
                entry.binding = 1;
                entry.texture = texview;
            }

            // sampler
            {
                auto& entry   = entries.at(2);
                entry.type    = GPUBindingResourceType::SAMPLER;
                entry.binding = 2;
                entry.sampler = sampler;
            }

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
        command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), color_attachment_state()));
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

TEST_CASE("rhi::vulkan::texture_sampling" * doctest::description("Rendering a textured triangle with the most basic graphics pipeline."))
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
    TextureSamplingApp(desc).run();
}

#ifdef WIN32
TEST_CASE("rhi::d3d12::texture_sampling" * doctest::description("Rendering a textured triangle with the most basic graphics pipeline."))
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
    TextureSamplingApp(desc).run();
}
#endif
