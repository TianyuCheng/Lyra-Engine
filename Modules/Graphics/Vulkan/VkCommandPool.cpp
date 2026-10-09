#include "VkUtils.h"

void VulkanCommandPool::init(uint queue_family_index)
{
    auto pool_info             = VkCommandPoolCreateInfo{};
    pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.pNext            = NULL;
    pool_info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = queue_family_index;

    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkCreateCommandPool(rhi->device, &pool_info, nullptr, &command_pool));
}

void VulkanCommandPool::reset(bool free)
{
    if (command_pool != VK_NULL_HANDLE) {
        auto rhi = get_rhi();
        vk_check(rhi->vtable.vkResetCommandPool(rhi->device, command_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
        primary.reset(command_pool, free);
        secondary.reset(command_pool, free);
    }
}

void VulkanCommandPool::destroy()
{
    reset();
    if (command_pool != VK_NULL_HANDLE) {
        auto rhi = get_rhi();
        rhi->vtable.vkDestroyCommandPool(rhi->device, command_pool, nullptr);
        command_pool = VK_NULL_HANDLE;
    }
}

VkCommandBuffer VulkanCommandPool::allocate(bool is_primary)
{
    if (command_pool == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;

    return is_primary
               ? primary.allocate(command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY)
               : secondary.allocate(command_pool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

VkCommandBuffer VulkanCommandPool::AllocatedCommandBuffers::allocate(VkCommandPool pool, VkCommandBufferLevel level)
{
    if (index < allocated.size())
        return allocated.at(index++);

    auto rhi = get_rhi();

    auto alloc_info               = VkCommandBufferAllocateInfo{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.pNext              = nullptr;
    alloc_info.commandPool        = pool;
    alloc_info.level              = level;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vk_check(rhi->vtable.vkAllocateCommandBuffers(rhi->device, &alloc_info, &command_buffer));
    index = static_cast<uint32_t>(allocated.size());
    allocated.push_back(command_buffer);
    return allocated.at(index++);
}

void VulkanCommandPool::AllocatedCommandBuffers::reset(VkCommandPool pool, bool free)
{
    index = 0;

    if (free && !allocated.empty()) {
        auto rhi   = get_rhi();
        uint count = static_cast<uint32_t>(allocated.size());
        rhi->vtable.vkFreeCommandBuffers(rhi->device, pool, count, allocated.data());
        allocated.clear();
    }
}
