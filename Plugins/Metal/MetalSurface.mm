#include "MetalUtils.h"

using namespace lyra;

bool api::create_surface(GPUSurfaceHandle& handle, const GPUSurfaceDescriptor& desc)
{
    auto rhi = get_rhi();

    // Create the swapchain (which wraps CAMetalLayer)
    auto obj = MetalSwapchain(desc);
    if (!obj.valid()) {
        get_logger()->error("Failed to create Metal surface/swapchain");
        return false;
    }

    auto ind = rhi->swapchains.add(obj);
    handle = GPUSurfaceHandle(ind);

    get_logger()->info("Metal surface created: {}x{}", obj.extent.width, obj.extent.height);
    return true;
}

void api::delete_surface(GPUSurfaceHandle handle)
{
    get_rhi()->swapchains.remove(handle.value);
}

bool api::get_surface_extent(GPUSurfaceHandle surface, GPUExtent2D& extent)
{
    auto rhi = get_rhi();
    auto& swapchain = fetch_resource(rhi->swapchains, surface);
    extent.width = swapchain.extent.width;
    extent.height = swapchain.extent.height;
    return true;
}

bool api::get_surface_format(GPUSurfaceHandle surface, GPUTextureFormat& format)
{
    auto rhi = get_rhi();
    auto& swapchain = fetch_resource(rhi->swapchains, surface);
    format = swapchain.rhi_format;
    return true;
}

uint api::get_surface_frames(GPUSurfaceHandle surface)
{
    auto rhi = get_rhi();
    auto& swapchain = fetch_resource(rhi->swapchains, surface);
    return static_cast<uint>(swapchain.frames.size());
}
