#include "MetalUtils.h"

using namespace lyra;

// Metal device creation and command queue setup
bool api::create_device(const GPUDeviceDescriptor& desc)
{
    @autoreleasepool {
        auto rhi = get_rhi();

        if (!rhi->device) {
            get_logger()->error("Metal device not initialized. Call create_adapter first.");
            return false;
        }

        // create command queues
        rhi->graphics_queue = [rhi->device newCommandQueue];
        rhi->compute_queue  = [rhi->device newCommandQueue];

        // transfer queue typically aliases graphics queue in Metal
        rhi->transfer_queue = rhi->graphics_queue;

        if (!rhi->graphics_queue || !rhi->compute_queue) {
            get_logger()->error("Failed to create Metal command queues");
            return false;
        }

        // set queue labels for debugging
        rhi->graphics_queue.label = @"Graphics Queue";
        rhi->compute_queue.label  = @"Compute Queue";

        // initialize frames (default to 3 frames in flight)
        rhi->frames.resize(3);
        for (uint i = 0; i < rhi->frames.size(); ++i) {
            rhi->frames[i].frame_id = i;
            rhi->frames[i].init(rhi->graphics_queue, rhi->compute_queue, rhi->transfer_queue);
        }

        rhi->current_frame_index = 0;
        rhi->current_image_index = 0;

        get_logger()->info("Metal device created with {} frames", rhi->frames.size());
        return true;
    }
}

void api::delete_device()
{
    @autoreleasepool {
        auto rhi = get_rhi();
        if (!rhi) return;

        // wait for all GPU work to complete before starting cleanup.
        rhi->wait_idle();

        // clean up remaining swapchains
        // needs to be deleted first, because it contains other handles
        for (auto& swapchain : rhi->swapchains)
            swapchain.destroy();

        // clean up remaining blases
        for (auto& blas : rhi->blases)
            blas.destroy();

        // clean up remaining tlases
        for (auto& tlas : rhi->tlases)
            tlas.destroy();

        // clean up remaining fences
        for (auto& frame : rhi->frames)
            frame.destroy();

        // clean up remaining fences
        for (auto& fence : rhi->fences)
            fence.destroy();

        // clean up remaining buffers
        for (auto& buffer : rhi->buffers)
            buffer.destroy();

        // clean up remaining texture views
        for (auto& view : rhi->views)
            view.destroy();

        // clean up remaining textures
        for (auto& texture : rhi->textures)
            texture.destroy();

        // clean up remaining samplers
        for (auto& sampler : rhi->samplers)
            sampler.destroy();

        // clean up remaining shaders
        for (auto& shader : rhi->shaders)
            shader.destroy();

        // clean up remaining bind group heaps
        for (auto& heap : rhi->bind_group_heaps)
            heap.destroy();

        // clean up remaining bind group layouts
        for (auto& layout : rhi->bind_group_layouts)
            layout.destroy();

        // clean up remaining pipeline layouts
        for (auto& layout : rhi->pipeline_layouts)
            layout.destroy();

        // clean up remaining pipelines
        for (auto& pipeline : rhi->pipelines)
            pipeline.destroy();

        // release queues (ARC handles this automatically).
        rhi->graphics_queue = nil;
        rhi->compute_queue  = nil;
        rhi->transfer_queue = nil;

        // finally, release the device itself.
        rhi->device = nil;

        get_logger()->info("Metal device deleted");
    }
}

// wait for device to be idle
void api::wait_idle()
{
    auto rhi = get_rhi();
    if (rhi) {
        rhi->wait_idle();
    }
}

// wait for fence
void api::wait_fence(GPUFenceHandle handle)
{
    auto  rhi   = get_rhi();
    auto& fence = fetch_resource(rhi->fences, handle);
    fence.wait();
}
