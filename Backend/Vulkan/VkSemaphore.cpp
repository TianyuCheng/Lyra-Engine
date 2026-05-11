#include "VkUtils.h"

/**
 * TODO: Semaphores are actually reusable, we don't have to create and destroy all the times,
 * we could reuse them instead.
 **/

VulkanSemaphore create_binary_semaphore()
{
    auto rhi = get_rhi();

    auto create_info  = VkSemaphoreCreateInfo{};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;

    VulkanSemaphore fence;
    vk_check(rhi->vtable.vkCreateSemaphore(rhi->device, &create_info, nullptr, &fence.semaphore));
    fence.type   = VK_SEMAPHORE_TYPE_BINARY;
    fence.target = 0;
    return fence;
}

VulkanSemaphore create_timeline_semaphore()
{
    auto rhi = get_rhi();

    auto timeline_create_info          = VkSemaphoreTypeCreateInfo{};
    timeline_create_info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_create_info.pNext         = nullptr;
    timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_create_info.initialValue  = 0;

    auto semaphore_create_info  = VkSemaphoreCreateInfo{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = &timeline_create_info;
    semaphore_create_info.flags = 0;

    VulkanSemaphore fence;
    vk_check(vkCreateSemaphore(rhi->device, &semaphore_create_info, NULL, &fence.semaphore));
    fence.type   = VK_SEMAPHORE_TYPE_TIMELINE;
    fence.target = 0;
    return fence;
}

VulkanSemaphore::VulkanSemaphore() : semaphore(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanSemaphore::VulkanSemaphore(VkSemaphoreType type)
{
    switch (type) {
        case VK_SEMAPHORE_TYPE_BINARY:
            *this = create_binary_semaphore();
            break;
        default:
            *this = create_timeline_semaphore();
    }
}

void VulkanSemaphore::wait(uint64_t timeout)
{
    auto wait_info           = VkSemaphoreWaitInfo{};
    wait_info.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait_info.pNext          = NULL;
    wait_info.flags          = VK_SEMAPHORE_WAIT_ANY_BIT;
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores    = &semaphore;
    wait_info.pValues        = &target;

    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkWaitSemaphores(rhi->device, &wait_info, timeout));

    // NOTE: It could also fail due to VK_TIMEOUT, maybe we want to tackle it.
}

void VulkanSemaphore::reset()
{
    auto rhi = get_rhi();

    uint64_t value;
    vk_check(rhi->vtable.vkGetSemaphoreCounterValue(rhi->device, semaphore, &value));
    target = value + 1;
}

bool VulkanSemaphore::ready()
{
    auto rhi = get_rhi();

    uint64_t value;
    vk_check(rhi->vtable.vkGetSemaphoreCounterValue(rhi->device, semaphore, &value));
    return target <= value;
}

void VulkanSemaphore::signal(uint64_t value)
{
    auto signal_info      = VkSemaphoreSignalInfo{};
    signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signal_info.pNext     = nullptr;
    signal_info.semaphore = semaphore;
    signal_info.value     = value;

    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkSignalSemaphore(rhi->device, &signal_info));
}

void VulkanSemaphore::destroy()
{
    if (semaphore != VK_NULL_HANDLE) {
        auto rhi = get_rhi();
        rhi->vtable.vkDestroySemaphore(rhi->device, semaphore, nullptr);
        semaphore = VK_NULL_HANDLE;
    }
}
