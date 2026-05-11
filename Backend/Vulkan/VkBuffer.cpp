#include "VkUtils.h"

VulkanBuffer::VulkanBuffer()
    : buffer(VK_NULL_HANDLE), alloc_info({})
{
    // do nothing
}

VulkanBuffer::VulkanBuffer(const GPUBufferDescriptor& desc, VkBufferUsageFlags additional_usages)
{
    auto rhi = get_rhi();

    VkBufferCreateInfo      buffer_create_info = {};
    VmaAllocationCreateInfo alloc_create_info  = {};

    // device buffer address
    if (desc.virtual_address)
        additional_usages |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // buffer create info
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size  = desc.size;
    buffer_create_info.usage = vkenum(desc.usage) | additional_usages;

    // allocation create info
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_create_info.flags = 0;

    bool dedicate_memory = true;

    if (desc.mapped_at_creation) {
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        dedicate_memory = false;
    }

    if (desc.usage.contains(GPUBufferUsage::MAP_READ)) {
        alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        dedicate_memory = false;
    }

    if (desc.usage.contains(GPUBufferUsage::MAP_WRITE)) {
        alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        dedicate_memory = false;
    }

    if (desc.usage.contains(GPUBufferUsage::UNIFORM)) {
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        dedicate_memory = false;
    }

    if (dedicate_memory) {
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    // create buffer
    vk_check(vmaCreateBuffer(
        rhi->alloc, &buffer_create_info, &alloc_create_info,
        &buffer, &allocation, &alloc_info));

    if (desc.mapped_at_creation) {
        mapped_data = reinterpret_cast<uint8_t*>(alloc_info.pMappedData);
        mapped_size = desc.size;
    }

    if (desc.virtual_address) {
        device_address = get_buffer_device_address(buffer);
    }

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, desc.label);
}

void VulkanBuffer::map(GPUSize64 offset, GPUSize64 size)
{
    if (mapped()) unmap();

    void* data = nullptr;
    vk_check(vmaMapMemory(get_rhi()->alloc, allocation, &data));
    mapped_data = reinterpret_cast<uint8_t*>(data) + offset;
    mapped_size = size == 0 ? alloc_info.size : size;
}

void VulkanBuffer::unmap()
{
    if (mapped_data) {
        vmaUnmapMemory(get_rhi()->alloc, allocation);
        mapped_data = nullptr;
        mapped_size = 0ull;
    }
}

void VulkanBuffer::destroy()
{
    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(get_rhi()->alloc, buffer, allocation);
        buffer = VK_NULL_HANDLE;
    }
}

VkDeviceAddress get_buffer_device_address(VkBuffer buffer)
{
    auto rhi    = get_rhi();
    auto info   = VkBufferDeviceAddressInfo{};
    info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.pNext  = nullptr;
    info.buffer = buffer;
    return vkGetBufferDeviceAddressKHR(rhi->device, &info);
}
