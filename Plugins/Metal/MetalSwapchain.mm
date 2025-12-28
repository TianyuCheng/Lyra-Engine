#include "MetalUtils.h"

using namespace lyra;

MetalSwapchain::MetalSwapchain()
{
    // do nothing
}

MetalSwapchain::MetalSwapchain(const GPUSurfaceDescriptor& in_desc) : desc(in_desc)
{
    auto rhi = get_rhi();

    // get CAMetalLayer from window handle (created by GLFW)
    metal_layer = (__bridge CAMetalLayer*)desc.window.native;
    if (!metal_layer) {
        get_logger()->error("Failed to get CAMetalLayer from window handle");
        return;
    }

    // configure the metal layer
    metal_layer.device          = rhi->device;
    metal_layer.pixelFormat     = MTLPixelFormatBGRA8Unorm; // standard swapchain format
    metal_layer.framebufferOnly = YES;

    // get initial extent from layer
    CGSize drawable_size = metal_layer.drawableSize;
    extent.width         = static_cast<uint>(drawable_size.width);
    extent.height        = static_cast<uint>(drawable_size.height);

    // store format info
    format     = metal_layer.pixelFormat;
    rhi_format = GPUTextureFormat::BGRA8UNORM;

    uint logic_frame_count = desc.frames;

    // create inflight fences (one per logical frame)
    inflight_fences.resize(logic_frame_count);
    for (uint i = 0; i < logic_frame_count; i++) {
        inflight_fences.at(i) = MetalFence(false);
    }

    // create image available semaphores (one per logical frame)
    image_available_semaphores.resize(logic_frame_count);
    for (uint i = 0; i < logic_frame_count; i++) {
        api::create_fence(image_available_semaphores.at(i));
    }

    // create render complete semaphores (one per logical frame)
    render_complete_semaphores.resize(logic_frame_count);
    for (uint i = 0; i < logic_frame_count; i++) {
        api::create_fence(render_complete_semaphores.at(i));
    }

    // initialize frame structures (drawable acquired per-frame)
    frames.resize(logic_frame_count);

    // create RHI frames if not already done
    uint existing_frames_count = static_cast<uint>(rhi->frames.size());
    if (existing_frames_count < desc.frames) {
        rhi->frames.resize(desc.frames);
        for (uint i = existing_frames_count; i < desc.frames; i++) {
            rhi->frames.at(i).init(rhi->graphics_queue, rhi->compute_queue, rhi->transfer_queue);
        }
    }
}

void MetalSwapchain::recreate()
{
    if (!metal_layer) return;

    // get updated extent from layer
    CGSize drawable_size = metal_layer.drawableSize;
    extent.width         = static_cast<uint>(drawable_size.width);
    extent.height        = static_cast<uint>(drawable_size.height);

    // clean up old frame textures/views
    for (auto& frame : frames) {
        frame.destroy();
    }
}

void MetalSwapchain::destroy()
{
    auto rhi = get_rhi();

    // destroy frames
    for (auto& frame : frames) {
        frame.destroy();
    }

    // destroy fences
    for (auto& fence : inflight_fences) {
        fence.destroy();
    }

    // destroy semaphores
    for (auto& semaphore : image_available_semaphores) {
        api::delete_fence(semaphore);
    }

    // destroy semaphores
    for (auto& semaphore : render_complete_semaphores) {
        api::delete_fence(semaphore);
    }

    frames.clear();
    inflight_fences.clear();
    image_available_semaphores.clear();
    render_complete_semaphores.clear();

    // Metal layer is owned by the view (GLFW), don't release it
    metal_layer = nil;
}

void MetalSwapchain::Frame::init(id<MTLTexture> tex)
{
    auto rhi = get_rhi();

    destroy();

    if (!tex) return;

    // create texture wrapper (swapchain textures are not owned by us)
    auto texture_obj    = MetalTexture{};
    texture_obj.texture = tex;
    texture_obj.format  = tex.pixelFormat;
    texture_obj.type    = tex.textureType;
    this->texture       = GPUTextureHandle(rhi->textures.add(texture_obj));

    // create texture view
    auto view_obj    = MetalTextureView{};
    view_obj.texture = tex;
    view_obj.format  = tex.pixelFormat;
    view_obj.type    = tex.textureType;
    this->view       = GPUTextureViewHandle(rhi->views.add(view_obj));
}

void MetalSwapchain::Frame::destroy()
{
    auto rhi = get_rhi();

    // clean up texture if already created
    if (this->texture.valid()) {
        // Don't destroy the underlying MTLTexture - it's owned by the drawable
        rhi->textures.remove(texture.value);
        this->texture.reset();
    }

    // clean up texture view if already created
    if (this->view.valid()) {
        rhi->views.remove(view.value);
        this->view.reset();
    }

    drawable = nil;
}

bool api::acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view,
    GPUFenceHandle& image_available_fence, GPUFenceHandle& render_complete_fence, bool& suboptimal)
{
    auto rhi = get_rhi();

    // initialize swapchain tracker
    if (rhi->surface_tracker.valid()) {
        assert(rhi->surface_tracker == surface && "Caller must call present_curr_frame() prior to calling acquire_next_frame() again!");
    }
    rhi->surface_tracker = surface;

    // query the swapchain
    auto& swp = fetch_resource(rhi->swapchains, surface);
    auto  ind = rhi->current_frame_index % swp.desc.frames;

    // swapchain sanity check
    assert(swp.valid());

    // check if metal layer size changed
    CGSize drawable_size = swp.metal_layer.drawableSize;
    if (static_cast<uint>(drawable_size.width) != swp.extent.width ||
        static_cast<uint>(drawable_size.height) != swp.extent.height) {
        swp.recreate();
        suboptimal = true;
    } else {
        suboptimal = false;
    }

    // query the current frame and assign synchronization primitives
    auto& frame                     = rhi->current_frame();
    frame.frame_id                  = rhi->current_frame_index;
    frame.inflight_fence            = swp.inflight_fences.at(ind);
    frame.image_available_semaphore = swp.image_available_semaphores.at(ind);
    frame.render_complete_semaphore = swp.render_complete_semaphores.at(ind);
    frame.existing_fences.push_back(frame.inflight_fence);

    // acquire next drawable from CAMetalLayer
    // Metal drawable acquisition is different from Vulkan - it blocks if no drawable available
    @autoreleasepool {
        id<CAMetalDrawable> drawable = [swp.metal_layer nextDrawable];
        if (!drawable) {
            get_logger()->error("Failed to acquire next drawable from CAMetalLayer");
            return false;
        }

        // Store drawable in frame
        rhi->current_image_index = ind;
        auto& swap_frame         = swp.frames.at(ind);
        swap_frame.drawable      = drawable;
        swap_frame.init(drawable.texture);

        // Update output handles
        texture = swap_frame.texture;
        view    = swap_frame.view;
    }

    // Update fence handles
    image_available_fence = frame.image_available_semaphore;
    render_complete_fence = frame.render_complete_semaphore;

    return true;
}

bool api::present_curr_frame(GPUSurfaceHandle surface)
{
    auto rhi = get_rhi();

    // validate swapchain tracker
    if (rhi->surface_tracker.valid()) {
        assert(rhi->surface_tracker == surface && "Caller must call acquire_next_frame() prior to calling present_curr_frame()!");
        rhi->surface_tracker.reset();
    }

    // query the swapchain
    auto& swp = fetch_resource(rhi->swapchains, surface);

    // swapchain sanity check
    assert(swp.valid());

    // get the current frame
    auto  ind        = rhi->current_image_index;
    auto& swap_frame = swp.frames.at(ind);

    // present the drawable
    if (swap_frame.drawable) {
        @autoreleasepool {
            [swap_frame.drawable present];
        }
        swap_frame.drawable = nil;
    }

    return true;
}
