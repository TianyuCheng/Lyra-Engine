#include "helper.h"

/**
 * @file main.cpp
 * @brief RHI validation test for compute shader using workgroup shared memory (groupshared/LDS)
 * and execution synchronization barriers (GroupMemoryBarrierWithGroupSync).
 *
 * Each 16x16 workgroup populates a local tile in groupshared memory:
 * `groupshared float4 s_tile[16][16];`
 * Synchronizes threads via `GroupMemoryBarrierWithGroupSync()`, and then performs an intra-tile
 * 3x3 box blur filtering pass reading exclusively from the shared memory buffer before writing
 * to the output 2D storage image.
 */

using namespace lyra;

namespace
{
    CString compute_shader_program = R"""(
import lyra;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> output_image;

groupshared float4 s_tile[32][32];

[shader("compute")]
[numthreads(32, 32, 1)]
void csmain(uint3 dispatch_id : SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID)
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
        float2 p = ndc - float2(-0.50, 0.50);
        float d = abs(p.x) + abs(p.y);
        if (d <= 0.08) {
            col = float4(0.00, 0.90, 0.46, 1.0); // Bright Mint
        } else if (d <= 0.15) {
            col = float4(0.00, 0.78, 0.33, 1.0); // Emerald
        } else if (d <= 0.22) {
            col = float4(0.11, 0.37, 0.13, 1.0); // Dark Forest
        }
    }
    // Quadrant 1: Top-Right Card (0.10..0.90, 0.10..0.90)
    else if (ndc.x >= 0.10 && ndc.x <= 0.90 && ndc.y >= 0.10 && ndc.y <= 0.90) {
        col = float4(0.082, 0.094, 0.133, 1.0); // Card color
        float2 p = ndc - float2(0.50, 0.50);
        float r = length(p);
        float theta = atan2(p.y, p.x);
        float star_r = 0.14 + 0.08 * cos(4.0 * theta);
        if (r <= star_r) {
            if (r <= 0.06) {
                col = float4(0.16, 0.47, 1.00, 1.0); // Electric Blue
            } else {
                col = float4(0.08, 0.40, 0.75, 1.0); // Deep Sapphire
            }
        }
    }
    // Quadrant 2: Bottom-Left Card (-0.90..-0.10, -0.90..-0.10)
    else if (ndc.x >= -0.90 && ndc.x <= -0.10 && ndc.y >= -0.90 && ndc.y <= -0.10) {
        col = float4(0.082, 0.094, 0.133, 1.0); // Card color
        float2 p_up = ndc - float2(-0.50, -0.36);
        float2 p_dn = ndc - float2(-0.50, -0.64);

        if (abs(p_up.x) <= 0.22 && abs(p_up.y - 0.5 * (0.22 - abs(p_up.x))) <= 0.04) {
            col = float4(1.00, 0.84, 0.00, 1.0); // Amber Gold
        } else if (abs(p_dn.x) <= 0.22 && abs(p_dn.y - 0.5 * (0.22 - abs(p_dn.x))) <= 0.04) {
            col = float4(1.00, 0.34, 0.13, 1.0); // Sunset Coral
        }
    }
    // Quadrant 3: Bottom-Right Card (0.10..0.90, -0.90..-0.10)
    else if (ndc.x >= 0.10 && ndc.x <= 0.90 && ndc.y >= -0.90 && ndc.y <= -0.10) {
        col = float4(0.082, 0.094, 0.133, 1.0); // Card color
        float2 p = ndc - float2(0.50, -0.50);
        float2 q = abs(p);
        float hex_d = max(q.x * 0.86602540 + q.y * 0.5, q.y);
        if (hex_d <= 0.10) {
            col = float4(1.00, 0.09, 0.27, 1.0); // Bright Ruby
        } else if (hex_d <= 0.20) {
            col = float4(0.76, 0.09, 0.36, 1.0); // Dark Magenta
        }
    }

    // 1. Populate workgroup shared memory tile
    uint lx = group_thread_id.x;
    uint ly = group_thread_id.y;
    s_tile[ly][lx] = col;

    // 2. Intra-workgroup synchronization barrier
    GroupMemoryBarrierWithGroupSync();

    // 3. Intra-tile 3x3 box blur filtering from shared memory
    float4 sum = float4(0.0);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int sx = clamp((int)lx + dx, 0, 31);
            int sy = clamp((int)ly + dy, 0, 31);
            sum += s_tile[sy][sx];
        }
    }

    output_image[pixel] = sum / 9.0;
}
)""";
} // namespace

struct ComputeSharedMemoryApp : public TestApp
{
    SimpleComputePipeline pipeline;
    GPUBindGroup          bind_group;
    GPUBindGroupHeap      heap;

    explicit ComputeSharedMemoryApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_compute_pipeline();
    }

    void setup_compute_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "compute_shared_memory_shader";
            desc.path   = "compute_shared_memory.slang";
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

        Array<GPUBindGroupEntry, 1> entries{};
        entries[0].type    = GPUResourceType::STORAGE_TEXTURE;
        entries[0].binding = 0;
        entries[0].texture = render_target.view.handle;

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
        cmd.dispatch_workgroups((desc.width + 31) / 32, (desc.height + 31) / 32, 1);

        // 3. Postprocess and copy storage texture to readback backbuffer
        postprocessing_compute(cmd, backbuffer.texture);

        cmd.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::compute_shared_memory" * doctest::description("Testing compute shader workgroup shared memory and barrier sync."))
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
    ComputeSharedMemoryApp(desc).run();
}
#endif

#if defined(_WIN32)
TEST_CASE("rhi::d3d12::compute_shared_memory" * doctest::description("Testing compute shader workgroup shared memory and barrier sync."))
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
    ComputeSharedMemoryApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::compute_shared_memory" * doctest::description("Testing compute shader workgroup shared memory and barrier sync."))
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
    ComputeSharedMemoryApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::compute_shared_memory" * doctest::description("Testing compute shader workgroup shared memory and barrier sync."))
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
    ComputeSharedMemoryApp(desc).run();
}
