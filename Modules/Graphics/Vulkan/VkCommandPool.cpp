#include "VkUtils.h"

#pragma region VulkanCommandPool
void VulkanCommandPool::init(uint queue_family_index)
{
    auto pool_info             = VkCommandPoolCreateInfo{};
    pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.pNext            = NULL;
    pool_info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = queue_family_index;

    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkCreateCommandPool(rhi->device, &pool_info, nullptr, &command_pool));

    auto fence_info  = VkFenceCreateInfo{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
    vk_check(rhi->vtable.vkCreateFence(rhi->device, &fence_info, nullptr, &fence));
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
    auto rhi = get_rhi();
    if (fence != VK_NULL_HANDLE) {
        rhi->vtable.vkDestroyFence(rhi->device, fence, nullptr);
        fence = VK_NULL_HANDLE;
    }
    if (command_pool != VK_NULL_HANDLE) {
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
#pragma endregion VulkanCommandPool

#pragma region VulkanCommandPoolManager
void VulkanCommandPoolManager::init(uint family_index)
{
    queue_family_index = family_index;
}

void VulkanCommandPoolManager::destroy()
{
    LockGuard lock(mutex);
    for (auto& pool : all_pools) {
        pool->destroy();
    }
    all_pools.clear();
    unused_pools.clear();
    inflight_pools.clear();
}

auto VulkanCommandPoolManager::acquire_pool() -> VulkanCommandPool*
{
    LockGuard lock(mutex);
    process_completed_pools();

    if (!unused_pools.empty()) {
        auto* pool = unused_pools.back();
        unused_pools.pop_back();
        return pool;
    }

    auto new_pool = make_own<VulkanCommandPool>();
    new_pool->init(queue_family_index);
    auto* pool_ptr = new_pool.get();
    all_pools.push_back(std::move(new_pool));
    return pool_ptr;
}

void VulkanCommandPoolManager::release_pool(VulkanCommandPool* pool, GPUCommandEncoderHandle cmd_handle, VkFence fence_to_wait)
{
    LockGuard lock(mutex);
    inflight_pools.push_back(InflightEntry{pool, cmd_handle, fence_to_wait});
}

void VulkanCommandPoolManager::process_completed_pools()
{
    auto rhi = get_rhi();
    for (usize i = 0; i < inflight_pools.size();) {
        auto& entry = inflight_pools[i];

        bool completed = false;
        if (entry.fence_to_wait == VK_NULL_HANDLE) {
            completed = true;
        } else {
            VkResult fence_status = rhi->vtable.vkGetFenceStatus(rhi->device, entry.fence_to_wait);
            if (fence_status == VK_SUCCESS) {
                completed = true;
            } else if (fence_status != VK_NOT_READY) {
                vk_check(fence_status);
            }
        }

        if (completed) {
            if (entry.pool && entry.pool->fence != VK_NULL_HANDLE && entry.fence_to_wait == entry.pool->fence) {
                vk_check(rhi->vtable.vkResetFences(rhi->device, 1, &entry.pool->fence));
            }

            if (entry.pool) {
                entry.pool->reset();
                unused_pools.push_back(entry.pool);
            }

            if (entry.cmd_handle.valid()) {
                LockGuard cmd_lock(rhi->command_buffers_mutex);
                rhi->command_buffers.remove(entry.cmd_handle.to_slotmap_handle<VulkanCommandBuffer>());
            }

            inflight_pools[i] = inflight_pools.back();
            inflight_pools.pop_back();
        } else {
            ++i;
        }
    }
}

void VulkanCommandPoolManager::wait_idle()
{
    LockGuard lock(mutex);

    auto rhi = get_rhi();
    for (auto& entry : inflight_pools) {
        if (entry.pool && entry.pool->fence != VK_NULL_HANDLE && entry.fence_to_wait == entry.pool->fence) {
            rhi->vtable.vkResetFences(rhi->device, 1, &entry.pool->fence);
        }
        if (entry.pool) {
            entry.pool->reset();
            unused_pools.push_back(entry.pool);
        }
        if (entry.cmd_handle.valid()) {
            LockGuard cmd_lock(rhi->command_buffers_mutex);
            rhi->command_buffers.remove(entry.cmd_handle.to_slotmap_handle<VulkanCommandBuffer>());
        }
    }
    inflight_pools.clear();
}
#pragma endregion VulkanCommandPoolManager
