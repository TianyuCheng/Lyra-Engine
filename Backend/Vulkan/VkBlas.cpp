#include "VkUtils.h"

VulkanBlas::VulkanBlas() : blas(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanBlas::VulkanBlas(const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors sizes)
{
    auto rhi = get_rhi();

    ranges.clear();
    geometries.clear();
    Vector<uint> max_primitive_counts;

    update_mode = desc.update_mode;

    for (auto& size : sizes) {
        ranges.push_back({});
        geometries.push_back({});

        // define geometry data
        auto& geometry        = geometries.back();
        geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.pNext        = nullptr;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;

        // triangle geometry data (will be partially overwritten by cmd::build_blas)
        geometry.geometry.triangles.sType                  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexData.hostAddress = nullptr;
        geometry.geometry.triangles.vertexStride           = size_of(vkenum(size.triangles.vertex_format));
        geometry.geometry.triangles.vertexFormat           = vkenum(size.triangles.vertex_format);
        geometry.geometry.triangles.maxVertex              = size.triangles.vertex_count - 1;
        geometry.geometry.triangles.indexData.hostAddress  = nullptr;
        geometry.geometry.triangles.indexType              = vkenum(size.triangles.index_format);

        // triangle build range (will be overwritten by cmd::build_blas)
        auto& range           = ranges.back();
        range.primitiveCount  = size.triangles.index_count / 3;
        range.primitiveOffset = 0;
        range.firstVertex     = 0;
        range.transformOffset = 0;

        // update max primitive count
        max_primitive_counts.push_back(range.primitiveCount);
    }

    // configure build info
    build.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = static_cast<uint32_t>(geometries.size());
    build.pGeometries   = geometries.data();
    build.flags         = vkenum(desc.flags);

    // get size requirements
    this->sizes       = VkAccelerationStructureBuildSizesInfoKHR{};
    this->sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    this->sizes.pNext = nullptr;
    rhi->vtable.vkGetAccelerationStructureBuildSizesKHR(rhi->device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build, max_primitive_counts.data(), &this->sizes);

    // create buffer to store BLAS
    auto additional             = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    auto buffer_desc            = GPUBufferDescriptor{};
    buffer_desc.usage           = 0;
    buffer_desc.size            = this->sizes.accelerationStructureSize;
    buffer_desc.virtual_address = true;
    storage                     = VulkanBuffer(buffer_desc, additional);

    // create BLAS vulkan object
    auto create_info   = VkAccelerationStructureCreateInfoKHR{};
    create_info.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.buffer = storage.buffer;
    create_info.size   = this->sizes.accelerationStructureSize;
    create_info.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    auto result = rhi->vtable.vkCreateAccelerationStructureKHR(rhi->device, &create_info, nullptr, &blas);
    if (result != VK_SUCCESS) destroy();
    vk_check(result);

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, (uint64_t)blas, desc.label);

    // save the blas address
    reference = lyra::execute([&]() {
        VkAccelerationStructureDeviceAddressInfoKHR info{};
        info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        info.accelerationStructure = blas;
        return vkGetAccelerationStructureDeviceAddressKHR(rhi->device, &info);
    });
}

void VulkanBlas::destroy()
{
    auto rhi = get_rhi();

    storage.destroy();

    if (blas != VK_NULL_HANDLE) {
        rhi->vtable.vkDestroyAccelerationStructureKHR(rhi->device, blas, nullptr);
        blas = VK_NULL_HANDLE;
    }
}
