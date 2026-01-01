#include "VkUtils.h"
#include <map>
#include <set>

static bool is_device_suitable(VkPhysicalDevice device, const std::vector<const char*>& requiredExtensions)
{
    auto               rhi     = get_rhi();
    QueueFamilyIndices indices = find_queue_family_indices(device, rhi->surface);

    if (!indices.graphics.has_value() || (rhi->surface && !indices.present.has_value())) {
        return false;
    }

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> required(requiredExtensions.begin(), requiredExtensions.end());

    for (const auto& extension : availableExtensions) {
        required.erase(extension.extensionName);
    }

    return required.empty();
}

static int calculate_device_score(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    int score = 0;

    // Feature richness is more important
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    auto has_extension = [&](const char* ext_name) {
        for (const auto& ext : availableExtensions) {
            if (strcmp(ext.extensionName, ext_name) == 0) {
                return true;
            }
        }
        return false;
    };

    if (has_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
        score += 2000;
    }
    if (has_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) && has_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)) {
        score += 5000;
    }
    if (features.samplerAnisotropy) {
        score += 500;
    }
    if (features.wideLines) {
        score += 100;
    }

    // Power/Performance
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 10000;
    } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 1000;
    }

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(device, &memProperties);
    VkDeviceSize local_memory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            local_memory += memProperties.memoryHeaps[i].size;
        }
    }
    score += local_memory / (1024 * 1024); // Score per MB

    score += properties.limits.maxImageDimension2D / 100;

    return score;
}

static void populate_device_properties(GPUSupportedLimits& limits)
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

static void populate_device_properties(GPUProperties& properties)
{
    auto rhi = get_rhi();

    const VkPhysicalDeviceLimits& vk_limits = rhi->props.limits;

    // texture row pitch alignment (buffer image properties)
    properties.texture_row_pitch_alignment = 4; // common minimum, may need device-specific query

    // push constant alignment
    properties.min_uniform_buffer_alignment = vk_limits.minUniformBufferOffsetAlignment;

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

    std::vector<const char*> requiredExtensions;
    if (rhi->surface) {
        requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    // Essential extensions for a modern renderer
    requiredExtensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    requiredExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    requiredExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    requiredExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    requiredExtensions.push_back(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME);

    std::multimap<int, VkPhysicalDevice> candidates;
    for (const auto& device : devices) {
        if (is_device_suitable(device, requiredExtensions)) {
            int score = calculate_device_score(device);
            candidates.insert(std::make_pair(score, device));
        }
    }

    if (candidates.empty()) {
        throw GPUInternalError("Failed to find a suitable GPU!");
    }

    rhi->adapter = candidates.rbegin()->second;

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
