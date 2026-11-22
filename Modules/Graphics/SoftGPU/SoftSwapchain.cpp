#include "SoftSwapchain.h"
#include "SoftDevice.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace lyra;
using namespace lyra::softgpu;

SoftSwapchain::SoftSwapchain(const GPUSurfaceDescriptor& desc, SoftResourceManager<SoftTexture>& texture_pool,
    SoftResourceManager<SoftTextureView>& view_pool, SoftResourceManager<SoftFence>& fence_pool)
    : window(desc.window), label(desc.label)
{
    format           = GPUTextureFormat::BGRA8UNORM;
    uint frame_count = std::max(2u, desc.frames);

#ifdef _WIN32
    if (window.native != nullptr) {
        RECT rect;
        if (GetClientRect(reinterpret_cast<HWND>(window.native), &rect)) {
            extent.width  = std::max(1u, static_cast<uint>(rect.right - rect.left));
            extent.height = std::max(1u, static_cast<uint>(rect.bottom - rect.top));
        }
    }
#endif

    frames.resize(frame_count);
    for (uint i = 0; i < frame_count; ++i) {
        // create texture for backbuffer
        GPUTextureDescriptor tex_desc{};
        tex_desc.size            = GPUExtent3D{extent.width, extent.height, 1};
        tex_desc.format          = format;
        tex_desc.dimension       = GPUTextureDimension::x2D;
        tex_desc.mip_level_count = 1;
        tex_desc.array_layers    = 1;
        tex_desc.sample_count    = 1;
        tex_desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::COPY_DST | GPUTextureUsage::COPY_SRC;
        tex_desc.label           = "swapchain_texture";

        auto  t_idx   = texture_pool.add(SoftTexture(tex_desc));
        auto  t_h     = GPUTextureHandle::create(t_idx);
        auto* tex_ptr = texture_pool.find(t_idx);

        // create view
        GPUTextureViewDescriptor view_desc{};
        view_desc.format            = format;
        view_desc.dimension         = GPUTextureViewDimension::x2D;
        view_desc.aspect            = GPUTextureAspect::COLOR;
        view_desc.base_mip_level    = 0;
        view_desc.mip_level_count   = 1;
        view_desc.base_array_layer  = 0;
        view_desc.array_layer_count = 1;
        view_desc.label             = "swapchain_view";

        auto v_idx = view_pool.add(SoftTextureView(t_h, view_desc, *tex_ptr));
        auto v_h   = GPUTextureViewHandle::create(v_idx);

        // create fences
        auto f1_idx = fence_pool.add(SoftFence());
        auto f2_idx = fence_pool.add(SoftFence());

        frames[i].texture         = t_h;
        frames[i].view            = v_h;
        frames[i].image_available = GPUFenceHandle::create(f1_idx);
        frames[i].render_complete = GPUFenceHandle::create(f2_idx);
    }
}

bool SoftSwapchain::acquire_next_frame(GPUTextureHandle& out_tex, GPUTextureViewHandle& out_view,
    GPUFenceHandle& out_available, GPUFenceHandle& out_complete, bool& suboptimal)
{
    if (frames.empty()) return false;

    current_frame_index = (current_frame_index + 1) % static_cast<uint>(frames.size());
    const auto& f       = frames[current_frame_index];

    out_tex       = f.texture;
    out_view      = f.view;
    out_available = f.image_available;
    out_complete  = f.render_complete;
    suboptimal    = false;
    return true;
}

bool SoftSwapchain::present(SoftResourceManager<SoftTexture>& texture_pool)
{
    if (frames.empty()) return false;
    const auto& f = frames[current_frame_index];

#ifdef _WIN32
    if (window.native != nullptr) {
        HWND hwnd = reinterpret_cast<HWND>(window.native);
        HDC  hdc  = GetDC(hwnd);
        if (hdc) {
            auto* tex = texture_pool.find(f.texture.to_slotmap_handle<SoftTexture>());
            if (tex && !tex->data.empty()) {
                BITMAPINFO bmi{};
                bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth       = static_cast<LONG>(extent.width);
                bmi.bmiHeader.biHeight      = -static_cast<LONG>(extent.height); // negative for top-down
                bmi.bmiHeader.biPlanes      = 1;
                bmi.bmiHeader.biBitCount    = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                StretchDIBits(hdc,
                    0, 0, extent.width, extent.height,
                    0, 0, extent.width, extent.height,
                    tex->data.data(),
                    &bmi,
                    DIB_RGB_COLORS,
                    SRCCOPY);
            }
            ReleaseDC(hwnd, hdc);
        }
    }
#endif

    return true;
}

void SoftSwapchain::destroy(SoftResourceManager<SoftTexture>& texture_pool, SoftResourceManager<SoftTextureView>& view_pool, SoftResourceManager<SoftFence>& fence_pool)
{
    for (auto& f : frames) {
        if (f.texture.valid()) {
            texture_pool.remove(f.texture.to_slotmap_handle<SoftTexture>());
            f.texture = {};
        }
        if (f.view.valid()) {
            view_pool.remove(f.view.to_slotmap_handle<SoftTextureView>());
            f.view = {};
        }
        if (f.image_available.valid()) {
            fence_pool.remove(f.image_available.to_slotmap_handle<SoftFence>());
            f.image_available = {};
        }
        if (f.render_complete.valid()) {
            fence_pool.remove(f.render_complete.to_slotmap_handle<SoftFence>());
            f.render_complete = {};
        }
    }
    frames.clear();
}

void SoftSwapchain::destroy()
{
    auto* dev = get_device();
    if (dev) {
        destroy(dev->textures, dev->texture_views, dev->fences);
    }
}
