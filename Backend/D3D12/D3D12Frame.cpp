#include "D3D12Utils.h"

constexpr uint MAX_SAMPLERS_HEAP_SIZE    = 2048u;
constexpr uint MAX_CBV_SRV_UAV_HEAP_SIZE = 1u << 16;

void D3D12Frame::init()
{
    // initialize command pools / allocators
    bundle_command_pool.init(D3D12_COMMAND_LIST_TYPE_BUNDLE);
    compute_command_pool.init(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    graphics_command_pool.init(D3D12_COMMAND_LIST_TYPE_DIRECT);
    transfer_command_pool.init(D3D12_COMMAND_LIST_TYPE_COPY);
}

void D3D12Frame::wait()
{
    for (auto& fence : existing_fences) {
        auto rhi = get_rhi();
        fetch_resource(rhi->fences, fence).wait();
    }
}

void D3D12Frame::reset()
{
    // reset all fences
    if (!existing_fences.empty()) {
        auto rhi = get_rhi();
        for (auto& fence : existing_fences)
            fetch_resource(rhi->fences, fence).reset();
    }

    // clean up command pools
    bundle_command_pool.reset();
    compute_command_pool.reset();
    graphics_command_pool.reset();
    transfer_command_pool.reset();
}

void D3D12Frame::free()
{
    // reset and free all commands
    for (auto& cmd : allocated_command_buffers) {
        cmd.cmd.reset();
        cmd.cmd.destroy();
    }

    // clean up existing allocated command buffers
    allocated_command_buffers.clear();

    existing_fences.clear();

    // clean up command pools
    bundle_command_pool.reset();
    compute_command_pool.reset();
    graphics_command_pool.reset();
    transfer_command_pool.reset();

    // NOTE: no need to reset descriptor pool,
    // descriptor pools are already reset every frame,
    // inserting extra resets will complicate the lifetime of descriptors.
    // default_heap.reset();
    // sampler_heap.reset();
    // dynamic_heap.free();
    // allocated_descriptors.clear();
}

void D3D12Frame::destroy()
{
    free();

    // destroy command pools
    bundle_command_pool.destroy();
    compute_command_pool.destroy();
    graphics_command_pool.destroy();
    transfer_command_pool.destroy();
}

GPUCommandEncoderHandle D3D12Frame::allocate(GPUQueueType type, bool primary)
{
    auto rhi = get_rhi();

    // set default descriptor heaps
    auto set_descriptor_heap = [&](D3D12CommandBuffer& command_buffer) {
        // D3D12 COPY QUEUE does not support SetDescriptorHeaps
        if (type != GPUQueueType::TRANSFER) {
            ID3D12DescriptorHeap* descriptor_heaps[] = {rhi->gpu_default_heap.heap, rhi->gpu_sampler_heap.heap};
            command_buffer.command_buffer->SetDescriptorHeaps(2, descriptor_heaps);
        }
    };

    // search from existing allocations
    for (uint i = 0; i < (uint)allocated_command_buffers.size(); i++) {
        auto& cmd = allocated_command_buffers.at(i);
        if (!cmd.used && cmd.type == type && cmd.primary == primary) {
            cmd.used = true;
            cmd.reset();
            set_descriptor_heap(cmd.cmd);
            return GPUCommandEncoderHandle(i);
        }
    }

    // new command buffer allocation
    uint handle = static_cast<uint>(allocated_command_buffers.size());
    allocated_command_buffers.push_back(CommandBuffer{});
    CommandBuffer& cmd = allocated_command_buffers.back();
    switch (type) {
        case GPUQueueType::TRANSFER:
            cmd.cmd.command_queue = rhi->transfer_queue;
            if (primary) {
                cmd.cmd.command_buffer    = transfer_command_pool.allocate();
                cmd.cmd.command_allocator = transfer_command_pool.command_allocator;
            } else {
                cmd.cmd.command_buffer    = bundle_command_pool.allocate();
                cmd.cmd.command_allocator = bundle_command_pool.command_allocator;
            }
            break;
        case GPUQueueType::COMPUTE:
            cmd.cmd.command_queue = rhi->compute_queue;
            if (primary) {
                cmd.cmd.command_buffer    = compute_command_pool.allocate();
                cmd.cmd.command_allocator = compute_command_pool.command_allocator;
            } else {
                cmd.cmd.command_buffer    = bundle_command_pool.allocate();
                cmd.cmd.command_allocator = bundle_command_pool.command_allocator;
            }
            break;
        case GPUQueueType::DEFAULT:
        default:
            cmd.cmd.command_queue = rhi->graphics_queue;
            if (primary) {
                cmd.cmd.command_buffer    = graphics_command_pool.allocate();
                cmd.cmd.command_allocator = graphics_command_pool.command_allocator;
            } else {
                cmd.cmd.command_buffer    = bundle_command_pool.allocate();
                cmd.cmd.command_allocator = bundle_command_pool.command_allocator;
            }
            break;
    }

    set_descriptor_heap(cmd.cmd);
    return GPUCommandEncoderHandle(handle);
}
