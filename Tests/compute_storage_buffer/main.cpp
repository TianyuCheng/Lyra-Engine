#include "helper.h"

/**
 * @file main.cpp
 * @brief RHI validation test for compute shader reading from a structured storage buffer (SSBO).
 *
 * Verifies compute pipeline creation, StructuredBuffer<T> binding (GPUResourceType::BUFFER with STORAGE),
 * 2D storage image write (RWTexture2D), workgroup dispatching, and host readback.
 *
 * The structured buffer provides procedural geometric parameters (colors, centers, radii, shape types)
 * for each of the four quadrants:
 * - Quadrant 0 (Top-Left): Concentric Mint / Emerald diamond emblem.
 * - Quadrant 1 (Top-Right): Radiant Electric Blue / Sapphire 4-pointed star.
 * - Quadrant 2 (Bottom-Left): Stacked Amber Gold / Sunset Coral chevrons.
 * - Quadrant 3 (Bottom-Right): Concentric Bright Ruby / Magenta hexagon badge.
 */

using namespace lyra;

namespace
{
    CString compute_shader_program = R"""(
import lyra;

struct QuadrantBadge {
    float4 primary_color;
    float4 secondary_color;
    float4 tertiary_color;
    float2 center;
    float radius;
    uint shape_type;
};

[[lyra::binding(0, 0)]]
RWTexture2D<float4> output_image;

[[lyra::binding(1, 0)]]
StructuredBuffer<QuadrantBadge> badges;

[shader("compute")]
[numthreads(16, 16, 1)]
void csmain(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_id.xy;
    if (pixel.x >= 640 || pixel.y >= 480) {
        return;
    }

    // Normalized coordinates
    float2 uv = (float2(pixel) + 0.5) / float2(640.0, 480.0);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y; // NDC Y positive upwards

    // Background: #0e1017
    float4 col = float4(0.055, 0.063, 0.090, 1.0);

    // Quadrant 0: Top-Left Card (-0.90..-0.10, 0.10..0.90)
    if (ndc.x >= -0.90 && ndc.x <= -0.10 && ndc.y >= 0.10 && ndc.y <= 0.90) {
        col = float4(0.082, 0.094, 0.133, 1.0); // Card color
        QuadrantBadge b = badges[0];
        float2 p = ndc - b.center;
        float d = abs(p.x) + abs(p.y);
        if (d <= b.radius * 0.36) {
            col = b.primary_color;
        } else if (d <= b.radius * 0.68) {
            col = b.secondary_color;
        } else if (d <= b.radius) {
            col = b.tertiary_color;
        }
    }
    // Quadrant 1: Top-Right Card (0.10..0.90, 0.10..0.90)
    else if (ndc.x >= 0.10 && ndc.x <= 0.90 && ndc.y >= 0.10 && ndc.y <= 0.90) {
        col = float4(0.082, 0.094, 0.133, 1.0);
        QuadrantBadge b = badges[1];
        float2 p = ndc - b.center;
        float r = length(p);
        float theta = atan2(p.y, p.x);
        float star_r = b.radius * (0.64 + 0.36 * cos(4.0 * theta));
        if (r <= star_r) {
            if (r <= b.radius * 0.28) {
                col = b.primary_color;
            } else {
                col = b.secondary_color;
            }
        }
    }
    // Quadrant 2: Bottom-Left Card (-0.90..-0.10, -0.90..-0.10)
    else if (ndc.x >= -0.90 && ndc.x <= -0.10 && ndc.y >= -0.90 && ndc.y <= -0.10) {
        col = float4(0.082, 0.094, 0.133, 1.0);
        QuadrantBadge b = badges[2];
        float2 p_up = ndc - (b.center + float2(0.0, 0.14));
        float2 p_dn = ndc - (b.center - float2(0.0, 0.14));

        if (abs(p_up.x) <= b.radius && abs(p_up.y - 0.5 * (b.radius - abs(p_up.x))) <= 0.04) {
            col = b.primary_color;
        } else if (abs(p_dn.x) <= b.radius && abs(p_dn.y - 0.5 * (b.radius - abs(p_dn.x))) <= 0.04) {
            col = b.secondary_color;
        }
    }
    // Quadrant 3: Bottom-Right Card (0.10..0.90, -0.90..-0.10)
    else if (ndc.x >= 0.10 && ndc.x <= 0.90 && ndc.y >= -0.90 && ndc.y <= -0.10) {
        col = float4(0.082, 0.094, 0.133, 1.0);
        QuadrantBadge b = badges[3];
        float2 p = ndc - b.center;
        float2 q = abs(p);
        float hex_d = max(q.x * 0.86602540 + q.y * 0.5, q.y);
        if (hex_d <= b.radius * 0.50) {
            col = b.primary_color;
        } else if (hex_d <= b.radius) {
            col = b.secondary_color;
        }
    }

    output_image[pixel] = col;
}
)""";
} // namespace

struct QuadrantBadgeData
{
    Vector4  primary_color;
    Vector4  secondary_color;
    Vector4  tertiary_color;
    Vector2  center;
    float    radius;
    uint32_t shape_type;
};

struct ComputeStorageBufferApp : public TestApp
{
    SimpleComputePipeline pipeline;
    GPUBindGroup          bind_group;
    GPUBindGroupHeap      heap;
    GPUBuffer             storage_buffer;

    explicit ComputeStorageBufferApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_storage_buffer();
        setup_compute_pipeline();
    }

    void setup_storage_buffer()
    {
        auto& device = RHI::get_current_device();

        Array<QuadrantBadgeData, 4> badges{};

        // Quadrant 0: Top-Left Concentric Diamond
        badges[0].primary_color   = Vector4(0.00f, 0.90f, 0.46f, 1.0f); // Mint
        badges[0].secondary_color = Vector4(0.00f, 0.78f, 0.33f, 1.0f); // Emerald
        badges[0].tertiary_color  = Vector4(0.11f, 0.37f, 0.13f, 1.0f); // Dark Forest
        badges[0].center          = Vector2(-0.50f, 0.50f);
        badges[0].radius          = 0.22f;
        badges[0].shape_type      = 0;

        // Quadrant 1: Top-Right Radiant Star
        badges[1].primary_color   = Vector4(0.16f, 0.47f, 1.00f, 1.0f); // Electric Blue
        badges[1].secondary_color = Vector4(0.08f, 0.40f, 0.75f, 1.0f); // Deep Sapphire
        badges[1].tertiary_color  = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        badges[1].center          = Vector2(0.50f, 0.50f);
        badges[1].radius          = 0.22f;
        badges[1].shape_type      = 1;

        // Quadrant 2: Bottom-Left Stacked Chevrons
        badges[2].primary_color   = Vector4(1.00f, 0.84f, 0.00f, 1.0f); // Amber Gold
        badges[2].secondary_color = Vector4(1.00f, 0.34f, 0.13f, 1.0f); // Sunset Coral
        badges[2].tertiary_color  = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        badges[2].center          = Vector2(-0.50f, -0.50f);
        badges[2].radius          = 0.22f;
        badges[2].shape_type      = 2;

        // Quadrant 3: Bottom-Right Concentric Hexagon
        badges[3].primary_color   = Vector4(1.00f, 0.09f, 0.27f, 1.0f); // Bright Ruby
        badges[3].secondary_color = Vector4(0.76f, 0.09f, 0.36f, 1.0f); // Dark Magenta
        badges[3].tertiary_color  = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        badges[3].center          = Vector2(0.50f, -0.50f);
        badges[3].radius          = 0.20f;
        badges[3].shape_type      = 3;

        auto buf_desc               = GPUBufferDescriptor{};
        buf_desc.label              = "badge storage buffer";
        buf_desc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
        buf_desc.size               = sizeof(badges);
        buf_desc.mapped_at_creation = true;

        storage_buffer = device.create_buffer(buf_desc);
        auto mapped    = storage_buffer.get_mapped_range<uint8_t>();
        std::memcpy(mapped.data, badges.data(), sizeof(badges));
        storage_buffer.unmap();
    }

    void setup_compute_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "compute_storage_buffer_shader";
            desc.path   = "compute_storage_buffer.slang";
            desc.source = compute_shader_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({
            {*module, "csmain"},
        });

        pipeline.init_cshader(device, module.get(), "csmain");
        pipeline.init_playout(device, reflection.get());
        pipeline.init_pipeline(device, reflection.get());

        auto heap_desc      = GPUBindGroupHeapDescriptor{};
        heap_desc.page_size = 64;
        heap                = device.create_bind_group_heap(heap_desc);

        Array<GPUBindGroupEntry, 2> entries{};
        entries[0].type    = GPUResourceType::STORAGE_TEXTURE;
        entries[0].binding = 0;
        entries[0].texture = render_target.view.handle;

        entries[1].type          = GPUResourceType::BUFFER;
        entries[1].binding       = 1;
        entries[1].buffer.buffer = storage_buffer.handle;
        entries[1].buffer.offset = 0;
        entries[1].buffer.size   = sizeof(QuadrantBadgeData) * 4;

        auto bg_desc    = GPUBindGroupDescriptor{};
        bg_desc.heap    = heap;
        bg_desc.layout  = pipeline.blayouts.at(0);
        bg_desc.entries = entries;
        bind_group      = device.create_bind_group(bg_desc);
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        auto cmd = execute([&]() {
            GPUCommandBufferDescriptor desc{};
            desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        if (desc.window) {
            cmd.wait(backbuffer.available, GPUBarrierSync::COMPUTE);
            cmd.signal(backbuffer.complete, GPUBarrierSync::COPY);
        }

        // 1. Transition render target texture to unordered access for compute
        cmd.resource_barrier(state_transition(render_target.texture, undefined_texture_state(), unordered_access_texture_state(GPUBarrierSync::COMPUTE)));

        // 2. Dispatch compute shader across workgroups
        cmd.set_pipeline(pipeline.pipeline);
        cmd.set_bind_group(0, bind_group);
        cmd.dispatch_workgroups((desc.width + 15) / 16, (desc.height + 15) / 16, 1);

        // 3. Postprocess and copy storage texture to readback backbuffer
        postprocessing_compute(cmd, backbuffer.texture);

        cmd.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::compute_storage_buffer" * doctest::description("Testing compute shader reading structured storage buffer."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ComputeStorageBufferApp(desc).run();
}
#endif

#if defined(_WIN32)
TEST_CASE("rhi::d3d12::compute_storage_buffer" * doctest::description("Testing compute shader reading structured storage buffer."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ComputeStorageBufferApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::compute_storage_buffer" * doctest::description("Testing compute shader reading structured storage buffer."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ComputeStorageBufferApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::compute_storage_buffer" * doctest::description("Testing compute shader reading structured storage buffer."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    ComputeStorageBufferApp(desc).run();
}
