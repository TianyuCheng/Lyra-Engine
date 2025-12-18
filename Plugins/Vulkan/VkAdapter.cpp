#include "VkUtils.h"

void populate_device_properties(GPUSupportedLimits& limits)
{
    auto rhi = get_rhi();

    const VkPhysicalDeviceLimits& vk_limits = rhi->props.limits;

    // texture dimensions
    limits.max_texture_dimension_1d = vk_limits.maxImageDimension1D;
    limits.max_texture_dimension_2d = vk_limits.maxImageDimension2D;
    limits.max_texture_dimension_3d = vk_limits.maxImageDimension3D;
    limits.max_texture_array_layers = vk_limits.maxImageArrayLayers;

    // descriptor sets and bindings (WebGPU bind groups ~ Vulkan descriptor sets)
    limits.max_bind_groups             = vk_limits.maxBoundDescriptorSets;
    limits.max_bindings_per_bind_group = std::min({
        vk_limits.maxDescriptorSetSamplers,
        vk_limits.maxDescriptorSetUniformBuffers,
        vk_limits.maxDescriptorSetStorageBuffers,
        vk_limits.maxDescriptorSetSampledImages,
        vk_limits.maxDescriptorSetStorageImages,
    });

    // dynamic buffers
    limits.max_dynamic_uniform_buffers_per_pipeline_layout = vk_limits.maxDescriptorSetUniformBuffersDynamic;
    limits.max_dynamic_storage_buffers_per_pipeline_layout = vk_limits.maxDescriptorSetStorageBuffersDynamic;

    // per-shader stage limits
    limits.max_sampled_textures_per_shader_stage = vk_limits.maxPerStageDescriptorSampledImages;
    limits.max_samplers_per_shader_stage         = vk_limits.maxPerStageDescriptorSamplers;
    limits.max_storage_buffers_per_shader_stage  = vk_limits.maxPerStageDescriptorStorageBuffers;
    limits.max_storage_textures_per_shader_stage = vk_limits.maxPerStageDescriptorStorageImages;
    limits.max_uniform_buffers_per_shader_stage  = vk_limits.maxPerStageDescriptorUniformBuffers;

    // buffer sizes and alignment
    limits.max_uniform_buffer_binding_size     = vk_limits.maxUniformBufferRange;
    limits.max_storage_buffer_binding_size     = vk_limits.maxStorageBufferRange;
    limits.min_uniform_buffer_offset_alignment = static_cast<uint>(vk_limits.minUniformBufferOffsetAlignment);
    limits.min_storage_buffer_offset_alignment = static_cast<uint>(vk_limits.minStorageBufferOffsetAlignment);

    // vertex attributes
    limits.max_vertex_buffers             = vk_limits.maxVertexInputBindings;
    limits.max_vertex_attributes          = vk_limits.maxVertexInputAttributes;
    limits.max_vertex_buffer_array_stride = vk_limits.maxVertexInputBindingStride;

    // general buffer size (use storage buffer range as approximation)
    limits.max_buffer_size = vk_limits.maxStorageBufferRange;

    // inter-stage variables (approximation)
    limits.max_inter_stage_shader_variables =
        std::min(
            vk_limits.maxVertexOutputComponents,
            vk_limits.maxFragmentInputComponents) /
        4; // Divide by 4 assuming vec4 components

    // color attachments
    limits.max_color_attachments                 = vk_limits.maxColorAttachments;
    limits.max_color_attachment_bytes_per_sample = 32; // Common max, may need adjustment

    // compute limits
    limits.max_compute_workgroup_storage_size    = vk_limits.maxComputeSharedMemorySize;
    limits.max_compute_invocations_per_workgroup = vk_limits.maxComputeWorkGroupInvocations;
    limits.max_compute_workgroup_size_x          = vk_limits.maxComputeWorkGroupSize[0];
    limits.max_compute_workgroup_size_y          = vk_limits.maxComputeWorkGroupSize[1];
    limits.max_compute_workgroup_size_z          = vk_limits.maxComputeWorkGroupSize[2];
    limits.max_compute_workgroups_per_dimension  = vk_limits.maxComputeWorkGroupCount[0];
}

void populate_device_properties(GPUProperties& properties)
{
    auto rhi = get_rhi();

    // texture row pitch alignment (buffer image properties)
    properties.texture_row_pitch_alignment = 4; // common minimum, may need device-specific query

    // subgroup properties (requires VK_KHR_shader_subgroup_extended_types or Vulkan 1.1+)
    if (rhi->props2.pNext) {
        // look for VkPhysicalDeviceSubgroupProperties in the pNext chain
        const VkPhysicalDeviceSubgroupProperties* subgroup_props = nullptr;
        const VkBaseInStructure*                  current        = reinterpret_cast<const VkBaseInStructure*>(rhi->props2.pNext);
        while (current) {
            if (current->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES) {
                subgroup_props = reinterpret_cast<const VkPhysicalDeviceSubgroupProperties*>(current);
                break;
            }
            current = current->pNext;
        }

        if (subgroup_props) {
            properties.subgroup_max_size = subgroup_props->subgroupSize;
            properties.subgroup_min_size = subgroup_props->subgroupSize; // Vulkan reports fixed size
        }
    }
}

bool api::create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor& descriptor)
{
    auto rhi = get_rhi();

    uint count;
    vk_check(vkEnumeratePhysicalDevices(rhi->instance, &count, nullptr));

    Vector<VkPhysicalDevice> devices(count);
    vk_check(vkEnumeratePhysicalDevices(rhi->instance, &count, devices.data()));

    // TODO: actually pick the most suitable adapter.
    rhi->adapter = devices.at(0);

    rhi->props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    rhi->props2.pNext = nullptr;

    // query some basic properties
    vkGetPhysicalDeviceProperties(rhi->adapter, &rhi->props);
    vkGetPhysicalDeviceProperties2(rhi->adapter, &rhi->props2);
    populate_device_properties(adapter.limits);
    populate_device_properties(adapter.properties);

    return true;
}

void api::delete_adapter()
{
    // Vulkan Physical Device does not need to be deleted
}
