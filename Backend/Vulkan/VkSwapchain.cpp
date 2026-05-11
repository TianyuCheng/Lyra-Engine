#include <algorithm>
#include "VkUtils.h"

#pragma region VulkanSwapchain
VulkanSwapchain::VulkanSwapchain()
{
    // do nothing
}

VulkanSwapchain::VulkanSwapchain(const GPUSurfaceDescriptor& desc, VkSurfaceKHR surface) : desc(desc), surface(surface)
{
    assert(surface && "Input VkSurfaceKHR is invalid!");

    recreate();

    uint image_frame_count = static_cast<uint>(frames.size());
    uint logic_frame_count = static_cast<uint>(desc.frames);

    // create inflight fences
    uint existing_fence_count = static_cast<uint>(inflight_fences.size());
    assert(existing_fence_count == 0u || existing_fence_count == logic_frame_count);
    if (existing_fence_count == 0u) {
        inflight_fences.resize(logic_frame_count);
        for (uint i = 0; i < logic_frame_count; i++)
            inflight_fences.at(i) = VulkanFence(false);
    }

    // create image available semaphores
    uint existing_image_available_semaphores = static_cast<uint>(image_available_semaphores.size());
    assert(existing_image_available_semaphores == 0u || existing_image_available_semaphores == logic_frame_count);
    if (existing_image_available_semaphores == 0u) {
        image_available_semaphores.resize(logic_frame_count);
        for (uint i = 0; i < logic_frame_count; i++)
            api::create_fence(image_available_semaphores.at(i), VK_SEMAPHORE_TYPE_BINARY);
    }

    // create render complete semaphores (each image must have its own render complete semaphore)
    uint existing_render_complete_semaphores = static_cast<uint>(render_complete_semaphores.size());
    assert(existing_render_complete_semaphores == 0u || existing_render_complete_semaphores == image_frame_count);
    if (existing_render_complete_semaphores == 0u) {
        render_complete_semaphores.resize(image_frame_count);
        for (uint i = 0; i < image_frame_count; i++)
            api::create_fence(render_complete_semaphores.at(i), VK_SEMAPHORE_TYPE_BINARY);
    }

    // create frames if not already done so
    auto rhi                   = get_rhi();
    uint existing_frames_count = static_cast<uint>(rhi->frames.size());
    if (existing_frames_count < desc.frames) {
        rhi->frames.resize(desc.frames);
        for (uint i = existing_frames_count; i < desc.frames; i++)
            rhi->frames.at(i).init();
    }
}

void VulkanSwapchain::recreate()
{
    auto rhi = get_rhi();

    SwapchainSupportDetails swapchain_support = query_swapchain_support(rhi->adapter, surface);
    VkExtent2D              swapchain_extent  = choose_swap_extent(desc, swapchain_support.capabilities);
    VkSurfaceFormatKHR      surface_format    = choose_swap_surface_format(swapchain_support.formats);
    VkPresentModeKHR        present_mode      = choose_swap_present_mode(swapchain_support.present_modes);

    uint32_t image_count = std::clamp(
        desc.frames,
        swapchain_support.capabilities.minImageCount,
        swapchain_support.capabilities.maxImageCount);

    auto create_info             = VkSwapchainCreateInfoKHR{};
    create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface          = surface;
    create_info.minImageCount    = image_count;
    create_info.imageFormat      = surface_format.format;
    create_info.imageColorSpace  = surface_format.colorSpace;
    create_info.imageExtent      = swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create_info.clipped          = VK_TRUE;
    create_info.presentMode      = present_mode;
    create_info.preTransform     = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha   = vkenum(desc.alpha_mode);

    auto indices              = find_queue_family_indices(rhi->adapter, surface);
    uint queueFamilyIndices[] = {indices.graphics.value(), indices.present.value()};
    if (indices.graphics != indices.present) {
        create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    this->colorspace = surface_format.colorSpace;
    this->format     = surface_format.format;
    this->extent     = swapchain_extent;

    VkSwapchainKHR old_swapchain = this->swapchain;
    if (this->swapchain != VK_NULL_HANDLE)
        create_info.oldSwapchain = old_swapchain;

    vk_check(rhi->vtable.vkCreateSwapchainKHR(rhi->device, &create_info, nullptr, &swapchain));

    // delete the old swapchain
    if (old_swapchain != VK_NULL_HANDLE)
        rhi->vtable.vkDestroySwapchainKHR(rhi->device, old_swapchain, nullptr);

    // get swapchain images
    uint count;
    vk_check(rhi->vtable.vkGetSwapchainImagesKHR(rhi->device, swapchain, &count, nullptr));

    Vector<VkImage> swapchain_images(count);
    vk_check(rhi->vtable.vkGetSwapchainImagesKHR(rhi->device, swapchain, &count, swapchain_images.data()));

    // set names for swapchain images
    for (uint i = 0; i < count; i++) {
        String name = "swapchain-" + std::to_string(i);
        rhi->set_debug_label(VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchain_images.at(i), name.c_str());
    }

    // create swapchain data
    assert(frames.size() == 0 || frames.size() == count);
    frames.resize(count);
    for (uint i = 0; i < count; i++)
        frames.at(i).init(swapchain_images.at(i), surface_format.format, extent);
}

void VulkanSwapchain::destroy()
{
    auto rhi = get_rhi();

    // destroy frames
    for (auto& frame : frames)
        frame.destroy();

    // destroy fences
    for (auto& fence : inflight_fences)
        fence.destroy();

    // destroy semaphores
    for (auto& semaphore : image_available_semaphores)
        api::delete_fence(semaphore);

    // destroy semaphores
    for (auto& semaphore : render_complete_semaphores)
        api::delete_fence(semaphore);

    // destroy swapchain
    if (swapchain != VK_NULL_HANDLE) {
        rhi->vtable.vkDestroySwapchainKHR(rhi->device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    // surface does not needs to be destroyed
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(rhi->instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    frames.clear();
    inflight_fences.clear();
    image_available_semaphores.clear();
    render_complete_semaphores.clear();
}
#pragma endregion VulkanSwapchain

#pragma region VulkanSwapchainFrame
void VulkanSwapchain::Frame::init(VkImage image, VkFormat format, VkExtent2D extent)
{
    auto rhi = get_rhi();

    destroy();

    // re-create texture
    auto texture    = VulkanTexture{};
    texture.image   = image;
    texture.format  = format;
    texture.aspects = VK_IMAGE_ASPECT_COLOR_BIT;
    texture.area    = extent;
    this->texture   = GPUTextureHandle::create(rhi->textures.add(texture));

    // re-create new texture view
    auto view = VulkanTextureView();
    view.area = extent; // record the render area

    auto create_info = VkImageViewCreateInfo{};
    {
        create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        create_info.image                           = image;
        create_info.format                          = format;
        create_info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel   = 0;
        create_info.subresourceRange.levelCount     = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount     = 1;
    }
    vk_check(rhi->vtable.vkCreateImageView(rhi->device, &create_info, nullptr, &view.view));
    this->view = GPUTextureViewHandle::create(rhi->views.add(view));
}

void VulkanSwapchain::Frame::destroy()
{
    auto rhi = get_rhi();

    // clean up texture if already created
    if (this->texture.valid()) {
        fetch_resource(rhi->textures, texture).destroy();
        rhi->textures.remove(texture.to_slotmap_handle<VulkanTexture>());
        this->texture.reset();
    }

    // clean up texture view if already created
    if (this->view.valid()) {
        fetch_resource(rhi->views, view).destroy();
        rhi->views.remove(view.to_slotmap_handle<VulkanTextureView>());
        this->view.reset();
    }
}
#pragma endregion VulkanSwapchainFrame

void default_swapchain_image_barrier(const VulkanSwapchain& swp, VkCommandBuffer command_buffer)
{
    auto rhi = get_rhi();

    auto frame   = swp.frames.at(rhi->current_image_index);
    auto texture = fetch_resource(rhi->textures, frame.texture);

    auto barrier                            = VkImageMemoryBarrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext                           = nullptr;
    barrier.srcAccessMask                   = VK_ACCESS_NONE;
    barrier.dstAccessMask                   = VK_ACCESS_NONE;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.image                           = texture.image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    rhi->vtable.vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_NONE,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}

void api::new_frame()
{
    auto rhi = get_rhi();

    // wait for the current frame to complete
    auto& frame = rhi->current_frame();

    // wait for inflight frame to complete
    frame.wait();
    frame.reset();

    // clear all existing fences
    frame.existing_fences.clear();
}

void api::end_frame()
{
    auto rhi = get_rhi();

    // increment the current frame index
    rhi->current_frame_index++;
}

// vulkan swapchain utils
bool api::acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view, GPUFenceHandle& image_available_fence, GPUFenceHandle& render_complete_fence, bool& suboptimal)
{
    auto rhi = get_rhi();

    // initialize swpachain tracker
    if (rhi->surface_tracker.valid()) {
        assert(rhi->surface_tracker == surface && "Caller must call present_curr_frame() prior to calling acquire_next_frame() again!");
        rhi->surface_tracker = surface;
    }

    // query the swapchain
    auto& swp = fetch_resource(rhi->swapchains, surface);
    auto  ind = rhi->current_frame_index % swp.desc.frames;

    // swapchain sanity check
    assert(swp.valid());

    // query the current frame (and assign the synchronization primitives for this swapchain)
    auto& frame                     = rhi->current_frame();
    frame.frame_id                  = rhi->current_frame_index;
    frame.inflight_fence            = swp.inflight_fences.at(ind);
    frame.image_available_semaphore = swp.image_available_semaphores.at(ind);
    frame.existing_fences.push_back(frame.inflight_fence.fence);

    // initialize suboptimal
    suboptimal = false;

    // acquire next frame
    auto semaphore = fetch_resource(rhi->fences, frame.image_available_semaphore);
    auto result    = rhi->vtable.vkAcquireNextImageKHR(rhi->device, swp.swapchain, UINT64_MAX, semaphore.semaphore, VK_NULL_HANDLE, &rhi->current_image_index);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        // recreate the swapchain if window resizes or moved to other displays
        api::wait_idle();
        swp.recreate();
        suboptimal = true;
        return true;
    }
    vk_check(result);

    // update swapchain view (this must be done after current_image_index is updated)
    auto& swap_frame = swp.frames.at(rhi->current_image_index);
    texture          = swap_frame.texture;
    view             = swap_frame.view;

    // track the render complete semaphore in frame (after acquiring the new image index)
    frame.render_complete_semaphore = swp.render_complete_semaphores.at(rhi->current_image_index);

    // update fences (this must be done after current_image_index is updated)
    image_available_fence = frame.image_available_semaphore;
    render_complete_fence = frame.render_complete_semaphore;
    return true;
}

bool api::present_curr_frame(GPUSurfaceHandle surface)
{
    auto rhi = get_rhi();

    // validator swpachain tracker
    if (rhi->surface_tracker.valid()) {
        assert(rhi->surface_tracker == surface && "Caller must call acquire_next_frame() prior to calling present_curr_frame()!");
        rhi->surface_tracker.reset();
    }

    // query the swapchain
    auto& swp = fetch_resource(rhi->swapchains, surface);

    // swapchain sanity check
    assert(swp.valid());

    // query the current frame (also update the frame index)
    auto& frame = rhi->current_frame();

    // query the semaphores
    auto& image_available_fence = fetch_resource(rhi->fences, frame.image_available_semaphore);
    auto& render_complete_fence = fetch_resource(rhi->fences, frame.render_complete_semaphore);

    // check if nothing has been down, insert dummy workloads if needed
    if (frame.allocated_command_buffers.empty()) {
        auto  command_buffer_handle = frame.allocate(GPUQueueType::COMPUTE, true);
        auto& command_buffer        = frame.allocated_command_buffers.at(command_buffer_handle.value);
        command_buffer.begin();
        default_swapchain_image_barrier(swp, command_buffer.command_buffer);
        command_buffer.end();
        command_buffer.wait(image_available_fence, GPUBarrierSync::NONE);
        command_buffer.signal(render_complete_fence, GPUBarrierSync::ALL);
        command_buffer.submit();
    }

    // present to swapchain
    auto present_info               = VkPresentInfoKHR{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext              = nullptr;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &render_complete_fence.semaphore;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &swp.swapchain;
    present_info.pImageIndices      = &rhi->current_image_index;
    present_info.pResults           = nullptr;

    VkResult result = rhi->vtable.vkQueuePresentKHR(rhi->present_queue, &present_info);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        // recreate the swapchain if window resizes or moved to other displays
        api::wait_idle();
        swp.recreate();
        return true;
    }

    vk_check(result);
    return true;
}
