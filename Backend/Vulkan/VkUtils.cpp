#include "VkUtils.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

static Logger logger = create_logger("Vulkan", LogLevel::info);

static VulkanRHI* VK_RHI = nullptr;

void set_rhi(VulkanRHI* instance)
{
    VK_RHI = instance;
}

auto get_rhi() -> VulkanRHI*
{
    return VK_RHI;
}

Logger get_logger()
{
    return logger;
}

// clang-format off
CString to_string(VkResult result)
{
    switch (result) {
        case VK_SUCCESS:                        return "[VkResult] Success!";
        case VK_NOT_READY:                      return "[VkResult] ERROR: A fence or query has not yet completed!";
        case VK_TIMEOUT:                        return "[VkResult] ERROR: A wait operation has not completed in the specified time!";
        case VK_EVENT_SET:                      return "[VkResult] ERROR: An event is signaled!";
        case VK_EVENT_RESET:                    return "[VkResult] ERROR: An event is unsignaled!";
        case VK_INCOMPLETE:                     return "[VkResult] ERROR: A return array was too small for the result!";
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return "[VkResult] ERROR: A host memory allocation has failed!";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "[VkResult] ERROR: A device memory allocation has failed!";
        case VK_ERROR_INITIALIZATION_FAILED:    return "[VkResult] ERROR: Initialization of an object could not be completed for implementation-specific reasons!";
        case VK_ERROR_DEVICE_LOST:              return "[VkResult] ERROR: The logical or physical device has been lost!";
        case VK_ERROR_MEMORY_MAP_FAILED:        return "[VkResult] ERROR: Mapping of a memory object has failed!";
        case VK_ERROR_LAYER_NOT_PRESENT:        return "[VkResult] ERROR: A requested layer is not present or could not be loaded!";
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return "[VkResult] ERROR: A requested extension is not supported!";
        case VK_ERROR_FEATURE_NOT_PRESENT:      return "[VkResult] ERROR: A requested feature is not supported!";
        case VK_ERROR_INCOMPATIBLE_DRIVER:      return "[VkResult] ERROR: The requested version of Vulkan is not supported by the driver or is otherwise incompatible!";
        case VK_ERROR_TOO_MANY_OBJECTS:         return "[VkResult] ERROR: Too many objects of the type have already been created!";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     return "[VkResult] ERROR: A requested format is not supported on this device!";
        case VK_ERROR_SURFACE_LOST_KHR:         return "[VkResult] ERROR: A surface is no longer available!";
        case VK_SUBOPTIMAL_KHR:                 return "[VkResult] ERROR: A swapchain no longer matches the surface properties exactly, but can still be used!";
        case VK_ERROR_OUT_OF_DATE_KHR:          return "[VkResult] ERROR: A surface has changed in such a way that it is no longer compatible with the swapchain!";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "[VkResult] ERROR: The display used by a swapchain does not use the same presentable image layout!";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "[VkResult] ERROR: The requested window is already connected to a VkSurfaceKHR, or to some other non-Vulkan API!";
        case VK_ERROR_VALIDATION_FAILED_EXT:    return "[VkResult] ERROR: A validation layer found an error!";
        default:                                return "[VkResult] ERROR: UNKNOWN VULKAN ERROR!";
    }
}
// clang-format on

// clang-format off
CString to_string(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:        return "Verbose";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:           return "Info";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:        return "Warning";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:          return "Error";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: return "MaxEnum";
    }
    return "Unknown";
}
// clang-format on

// clang-format off
CString to_string(VkDebugUtilsMessageTypeFlagsEXT type) {
    switch (type) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:        return "General";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:     return "Validation";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:    return "Performance";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_FLAG_BITS_MAX_ENUM_EXT: return "MaxEnum";
    }
    return "Unknown";
}

bool has_portability_subset(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());
    for (const auto& extension : availableExtensions) {
        if (std::string(extension.extensionName) == "VK_KHR_portability_subset") {
            return true;
        }
    }
    return false;
}

HashSet<String> get_supported_instance_extensions()
{
    uint count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr); // get number of extensions
    Vector<VkExtensionProperties> extensions(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()); // populate buffer
    HashSet<String> results;
    for (auto& extension : extensions) {
        results.insert(extension.extensionName);
    }
    return results;
}

HashSet<String> get_supported_device_extensions(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    HashSet<String> extensions;
    for (const auto& extension : availableExtensions) {
        extensions.emplace(extension.extensionName);
    }
    return extensions;
}

QueueFamilyIndices find_queue_family_indices(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    uint queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    QueueFamilyIndices indices;

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // check graphics queue family
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics = i;

        // check compute queue family
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            indices.compute = i;

        // check transfer queue family
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            indices.transfer = i;

        // check present queue family
        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) indices.present = i;
        }

        if (surface != VK_NULL_HANDLE) {
            // check for all queue families
            if (indices.graphics.has_value() &&
                indices.compute.has_value() &&
                indices.present.has_value()) {
                break;
            }
        } else {
            // check for only compute and graphics families
            if (indices.graphics.has_value() &&
                indices.compute.has_value()) {
                break;
            }
        }

        i++;
    }
    return indices;
}

uint infer_texture_row_length(VkFormat format, uint bytes_per_row)
{
    // tightly packed
    if (bytes_per_row == 0) return 0;

    // otherwise convert it to number of texels
    uint texel_size = size_of(format);
    return (bytes_per_row + texel_size - 1) / texel_size;
}

SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice adapter, VkSurfaceKHR surface)
{
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(adapter, surface, &details.capabilities);

    uint format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(adapter, surface, &format_count, nullptr);

    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(adapter, surface, &format_count, details.formats.data());
    }

    uint present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(adapter, surface, &present_mode_count, nullptr);

    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(adapter, surface, &present_mode_count, details.present_modes.data());
    }

    return details;
}

VkSurfaceFormatKHR choose_swap_surface_format(const Vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats.at(0);
}

VkPresentModeKHR choose_swap_present_mode(const Vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_extent(const GPUSurfaceDescriptor& desc, const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    // query window size
    uint width, height;
    Window::api()->get_window_size(desc.window, width, height);

    // query window scale
    float fb_xscale, fb_yscale;
    Window::api()->get_framebuffer_scale(desc.window, fb_xscale, fb_yscale);

    VkExtent2D actual_extent;
    actual_extent.width  = static_cast<uint>(width * fb_xscale);
    actual_extent.height = static_cast<uint>(height * fb_yscale);
    actual_extent.width  = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actual_extent;
}
