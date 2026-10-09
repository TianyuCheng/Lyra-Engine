#include "VkUtils.h"

VulkanFence::VulkanFence() : fence(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanFence::VulkanFence(bool signaled)
{
    auto rhi = get_rhi();

    auto create_info  = VkFenceCreateInfo{};
    create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    vk_check(rhi->vtable.vkCreateFence(rhi->device, &create_info, nullptr, &fence));
}

void VulkanFence::destroy()
{
    if (fence != VK_NULL_HANDLE) {
        auto rhi = get_rhi();
        rhi->vtable.vkDestroyFence(rhi->device, fence, nullptr);
    }
}

void VulkanFence::reset()
{
    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkResetFences(rhi->device, 1, &fence));
}

void VulkanFence::wait(uint64_t timeout)
{
    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkWaitForFences(rhi->device, 1, &fence, VK_TRUE, timeout));
}
