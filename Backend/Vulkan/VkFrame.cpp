#include "VkUtils.h"

void VulkanFrame::init()
{
    api::create_fence(image_available_semaphore, VK_SEMAPHORE_TYPE_BINARY);

    auto rhi = get_rhi();

    if (rhi->queues.compute.has_value())
        compute_command_pool.init(rhi->queues.compute.value());

    if (rhi->queues.graphics.has_value())
        graphics_command_pool.init(rhi->queues.graphics.value());

    if (rhi->queues.transfer.has_value())
        transfer_command_pool.init(rhi->queues.transfer.value());
}

void VulkanFrame::wait()
{
    if (!existing_fences.empty()) {
        auto rhi = get_rhi();
        uint cnt = static_cast<uint>(existing_fences.size());
        rhi->vtable.vkWaitForFences(rhi->device, cnt, existing_fences.data(), VK_TRUE, UINT64_MAX);
    }
}

void VulkanFrame::reset()
{
    // release any resources that is owned by this command buffer
    for (auto& command_buffer : allocated_command_buffers)
        command_buffer.reset();

    // reset all fences
    if (!existing_fences.empty()) {
        auto rhi = get_rhi();
        uint cnt = static_cast<uint>(existing_fences.size());
        rhi->vtable.vkResetFences(rhi->device, cnt, existing_fences.data());
    }

    compute_command_pool.reset();
    graphics_command_pool.reset();
    transfer_command_pool.reset();
    allocated_command_buffers.clear();
}

void VulkanFrame::free()
{
    // release any resources that is owned by this command buffer
    for (auto& command_buffer : allocated_command_buffers)
        command_buffer.reset();

    // reset and free all commands
    compute_command_pool.reset(true);
    graphics_command_pool.reset(true);
    transfer_command_pool.reset(true);
    allocated_command_buffers.clear();
}

GPUCommandEncoderHandle VulkanFrame::allocate(GPUQueueType type, bool primary)
{
    auto rhi = get_rhi();

    VulkanCommandBuffer command_buffer;
    command_buffer.frame_id = frame_id;
    switch (type) {
        case GPUQueueType::DEFAULT:
            command_buffer.command_queue  = rhi->graphics_queue;
            command_buffer.command_buffer = graphics_command_pool.allocate(primary);
            break;
        case GPUQueueType::COMPUTE:
            command_buffer.command_queue  = rhi->compute_queue;
            command_buffer.command_buffer = compute_command_pool.allocate(primary);
            break;
        case GPUQueueType::TRANSFER:
            command_buffer.command_queue  = rhi->transfer_queue;
            command_buffer.command_buffer = transfer_command_pool.allocate(primary);
            break;
    }

    uint index  = static_cast<uint>(allocated_command_buffers.size());
    auto handle = GPUCommandEncoderHandle(index);
    allocated_command_buffers.push_back(command_buffer);
    return handle;
}

void VulkanFrame::destroy()
{
    compute_command_pool.destroy();
    graphics_command_pool.destroy();
    transfer_command_pool.destroy();
}
