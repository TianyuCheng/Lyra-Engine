#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTSWAPCHAIN_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTSWAPCHAIN_H

#include "SoftCommon.h"
#include "SoftTexture.h"
#include "SoftFence.h"

namespace lyra::softgpu
{
    struct SoftSwapchainFrame
    {
        GPUTextureHandle     texture;
        GPUTextureViewHandle view;
        GPUFenceHandle       image_available;
        GPUFenceHandle       render_complete;
    };

    struct SoftSwapchain
    {
        WindowHandle               window{};
        GPUExtent2D                extent{1280, 720};
        GPUTextureFormat           format              = GPUTextureFormat::BGRA8UNORM;
        uint32_t                   current_frame_index = 0;
        Vector<SoftSwapchainFrame> frames;
        CString                    label = "";

        SoftSwapchain() = default;
        SoftSwapchain(const GPUSurfaceDescriptor& desc, Slotmap<SoftTexture>& texture_pool,
            Slotmap<SoftTextureView>& view_pool, Slotmap<SoftFence>& fence_pool);

        auto acquire_next_frame(GPUTextureHandle& out_tex, GPUTextureViewHandle& out_view,
            GPUFenceHandle& out_available, GPUFenceHandle& out_complete, bool& suboptimal) -> bool;
        auto present(Slotmap<SoftTexture>& texture_pool) -> bool;
        auto destroy(Slotmap<SoftTexture>& texture_pool, Slotmap<SoftTextureView>& view_pool, Slotmap<SoftFence>& fence_pool) -> void;
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTSWAPCHAIN_H
