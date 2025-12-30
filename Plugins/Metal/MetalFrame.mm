#include "MetalUtils.h"
using namespace lyra;

void MetalFrame::init(id<MTLCommandQueue> graphics, id<MTLCommandQueue> compute, id<MTLCommandQueue> transfer)
{
    graphics_command_pool.init(graphics);
    compute_command_pool.init(compute);
    transfer_command_pool.init(transfer);
}

void MetalFrame::wait()
{
    for (auto& fence : existing_fences) {
        fence.wait();
    }
}

void MetalFrame::reset()
{
    // reset any resources owned by command buffers
    for (auto& command_buffer : allocated_command_buffers) {
        [command_buffer.command_buffer waitUntilCompleted];
        NSLog(@"[After Commit] Reference count is %ld", CFGetRetainCount((__bridge CFTypeRef)command_buffer.command_buffer));

        command_buffer.reset();
    }
    allocated_command_buffers.clear();
    existing_fences.clear();
}

void MetalFrame::free()
{
    // reset any resources owned by command buffers
    for (auto& command_buffer : allocated_command_buffers) {
        command_buffer.reset();
    }
    allocated_command_buffers.clear();
}

void MetalFrame::destroy()
{
    graphics_command_pool.destroy();
    compute_command_pool.destroy();
    transfer_command_pool.destroy();
    allocated_command_buffers.clear();
}

GPUCommandEncoderHandle MetalFrame::allocate(GPUQueueType type, bool primary)
{
    @autoreleasepool {
        MetalCommandBuffer cmd;
        cmd.frame_id = frame_id;

        // select the appropriate command pool based on queue type
        switch (type) {
            case GPUQueueType::DEFAULT:
                cmd.command_queue  = graphics_command_pool.command_queue;
                cmd.command_buffer = graphics_command_pool.allocate();
                break;
            case GPUQueueType::COMPUTE:
                cmd.command_queue  = compute_command_pool.command_queue;
                cmd.command_buffer = compute_command_pool.allocate();
                break;
            case GPUQueueType::TRANSFER:
                cmd.command_queue  = transfer_command_pool.command_queue;
                cmd.command_buffer = transfer_command_pool.allocate();
                break;
        }

        uint index = static_cast<uint>(allocated_command_buffers.size());
        allocated_command_buffers.push_back(cmd);
        return GPUCommandEncoderHandle(index);
    }
}

void api::new_frame()
{
    auto rhi = get_rhi();
    rhi->current_frame_index++;
    rhi->current_frame().reset();
}

void api::end_frame() {}
