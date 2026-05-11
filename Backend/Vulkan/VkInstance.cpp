#include "VkUtils.h"

void fill_vulkan_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& info)
{
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = vulkan_debug_callback;
    info.pNext           = nullptr;
    info.flags           = 0;
}

bool api::create_instance(const RHIDescriptor& desc)
{
    auto logger = get_logger();

    Vector<const char*> instance_extensions = {};
    Vector<const char*> debugger_extensions = {};
    Vector<const char*> validation_layers   = {};

    // surface khr
    add_surface_extension(instance_extensions);

    // add debug utils extension
    if (desc.flags.contains(RHIFlag::DEBUG)) {
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        debugger_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }

    // add validation layer
    if (desc.flags.contains(RHIFlag::VALIDATION)) {
        validation_layers.push_back(LUNARG_VALIDATION_LAYER_NAME);
    }

    // load extensions
    instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    // logger
    if (!instance_extensions.empty()) {
        logger->debug("Create VkInstance with extensions:");
        for (const auto& extension : instance_extensions)
            logger->debug("- {}", extension);
    }
    if (!debugger_extensions.empty()) {
        logger->debug("Create VkInstance with debug extensions:");
        for (const auto& extension : debugger_extensions)
            logger->debug("- {}", extension);
    }
    if (!validation_layers.empty()) {
        logger->debug("Create VkInstance with validation layers:");
        for (const auto& layer : validation_layers)
            logger->debug("- {}", layer);
    }

    // check availability of extensions
    auto supported_extensions        = get_supported_instance_extensions();
    uint unsatisfied_extension_count = 0;
    for (auto& extension : instance_extensions) {
        auto ext = String(extension);
        if (supported_extensions.find(ext) == supported_extensions.end()) {
            logger->error("VkInstance Extension <{}> is not supported!", ext);
            unsatisfied_extension_count++;
        }
    }
    if (unsatisfied_extension_count) {
        show_error("Vulkan Render Backend", "Missing required instance extensions!");
        return false;
    }

    // prepare application info
    VkApplicationInfo app_info  = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "Lyra Engine";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "Lyra Engine";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_3;

    // prepare instance info
    VkInstanceCreateInfo create_info    = {};
    create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo        = &app_info;
    create_info.enabledExtensionCount   = static_cast<uint>(instance_extensions.size());
    create_info.ppEnabledExtensionNames = instance_extensions.data();
    create_info.flags                   = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    // prepare debug info
    VkDebugUtilsMessengerCreateInfoEXT debug_info;
    if (desc.flags.contains(RHIFlag::VALIDATION)) {
        create_info.enabledLayerCount   = static_cast<uint>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
        fill_vulkan_debug_messenger_create_info(debug_info);
        create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_info;
    } else {
        create_info.enabledLayerCount = 0;
        create_info.pNext             = nullptr;
    }

    // create vulkan instance
    VkInstance vkinst;
    vk_check(vkCreateInstance(&create_info, nullptr, &vkinst));

    // load instsance functions
    volkLoadInstance(vkinst);

    // copy to instance pointer
    auto rhi      = new VulkanRHI{};
    rhi->rhiflags = desc.flags;
    rhi->instance = vkinst;
    rhi->surface  = create_surface(vkinst, desc.window);
    set_rhi(rhi);
    return true;
}

void api::delete_instance()
{
    auto rhi = get_rhi();
    if (!rhi) return;

    if (rhi->device) {
        vkDestroyDevice(rhi->device, nullptr);
        rhi->device = VK_NULL_HANDLE;
    }

    if (rhi->surface) {
        vkDestroySurfaceKHR(rhi->instance, rhi->surface, nullptr);
        rhi->surface = VK_NULL_HANDLE;
    }

    if (rhi->instance) {
        vkDestroyInstance(rhi->instance, nullptr);
        rhi->instance = VK_NULL_HANDLE;
    }

    delete rhi;
    set_rhi(nullptr);
}
