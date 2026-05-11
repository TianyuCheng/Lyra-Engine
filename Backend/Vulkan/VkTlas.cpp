#include <Lyra/Common/Function.h>

#include "VkUtils.h"

VulkanTlas::VulkanTlas() : tlas(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanTlas::VulkanTlas(const GPUTlasDescriptor& desc)
{
    auto rhi = get_rhi();

    max_instance_count = desc.max_instances;

    update_mode = desc.update_mode;

    // create instance buffer
    instances = execute([&]() {
        auto desc            = GPUBufferDescriptor{};
        desc.size            = sizeof(VkAccelerationStructureInstanceKHR) * max_instance_count;
        desc.usage           = GPUBufferUsage::TLAS_INPUT | GPUBufferUsage::COPY_DST;
        desc.virtual_address = true;
        return VulkanBuffer(desc);
    });

    // create staging buffer
    staging = execute([&]() {
        auto desc  = GPUBufferDescriptor{};
        desc.size  = sizeof(VkAccelerationStructureInstanceKHR) * max_instance_count;
        desc.usage = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
        return VulkanBuffer(desc);
    });

    // setup geometry for instances
    geometry.sType                                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags                                 = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers    = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instances.device_address;

    // setup build info
    build.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build.flags         = vkenum(desc.update_mode);
    build.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries   = &geometry;
    build.flags         = vkenum(desc.flags);

    // setup build range info
    range.primitiveCount  = max_instance_count;
    range.primitiveOffset = 0;
    range.firstVertex     = 0;
    range.transformOffset = 0;

    // get size requirements
    this->sizes       = VkAccelerationStructureBuildSizesInfoKHR{};
    this->sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    this->sizes.pNext = nullptr;
    rhi->vtable.vkGetAccelerationStructureBuildSizesKHR(rhi->device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build, &range.primitiveCount, &this->sizes);

    // create storage buffer
    auto storage_buffer_desc            = GPUBufferDescriptor{};
    storage_buffer_desc.size            = this->sizes.accelerationStructureSize;
    storage_buffer_desc.usage           = GPUBufferUsage::TLAS_INPUT | GPUBufferUsage::COPY_DST;
    storage_buffer_desc.virtual_address = true;
    storage                             = VulkanBuffer(storage_buffer_desc, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);

    // Create TLAS acceleration structure
    auto create_info   = VkAccelerationStructureCreateInfoKHR{};
    create_info.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.buffer = storage.buffer;
    create_info.size   = this->sizes.accelerationStructureSize;
    create_info.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    auto result = rhi->vtable.vkCreateAccelerationStructureKHR(rhi->device, &create_info, nullptr, &tlas);
    if (result != VK_SUCCESS) destroy();
    vk_check(result);

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, (uint64_t)tlas, desc.label);
}

void VulkanTlas::destroy()
{
    auto rhi = get_rhi();

    storage.destroy();
    staging.destroy();
    instances.destroy();

    if (tlas != VK_NULL_HANDLE) {
        rhi->vtable.vkDestroyAccelerationStructureKHR(rhi->device, tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
}
