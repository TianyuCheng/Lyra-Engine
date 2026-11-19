#include "VkUtils.h"

void VulkanFrame::init()
{
    api::create_fence(image_available_semaphore, VK_SEMAPHORE_TYPE_BINARY);
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
    // reset all fences
    if (!existing_fences.empty()) {
        auto rhi = get_rhi();
        uint cnt = static_cast<uint>(existing_fences.size());
        rhi->vtable.vkResetFences(rhi->device, cnt, existing_fences.data());
    }
    existing_fences.clear();
    frame_has_work = false;
}

void VulkanFrame::free()
{
    reset();
}

void VulkanFrame::destroy()
{
    reset();
}
