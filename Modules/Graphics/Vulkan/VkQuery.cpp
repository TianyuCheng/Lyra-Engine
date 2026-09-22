#include "VkUtils.h"

VulkanQuerySet::VulkanQuerySet() : pool(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanQuerySet::VulkanQuerySet(const GPUQuerySetDescriptor& desc) : pool(VK_NULL_HANDLE)
{
    auto create_info               = VkQueryPoolCreateInfo{};
    create_info.sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    create_info.pNext              = nullptr;
    create_info.queryType          = vkenum(desc.type);
    create_info.queryCount         = desc.count;
    create_info.flags              = 0;
    create_info.pipelineStatistics = 0;

    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkCreateQueryPool(rhi->device, &create_info, nullptr, &pool));

    // record basic info
    type  = create_info.queryType;
    count = create_info.queryCount;
}

void VulkanQuerySet::destroy()
{
    if (pool != VK_NULL_HANDLE) {
        auto rhi = get_rhi();
        rhi->vtable.vkDestroyQueryPool(rhi->device, pool, nullptr);
        pool = VK_NULL_HANDLE;
    }
}
