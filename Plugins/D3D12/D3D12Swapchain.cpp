// reference: https://alain.xyz/blog/raw-directx12
#include <Lyra/Window/WSIAPI.h>
#include <Lyra/Window/WSITypes.h>

#include "D3D12Utils.h"

#pragma region D3D12Swapchain
D3D12Swapchain::D3D12Swapchain()
{
    // do nothing
}

D3D12Swapchain::D3D12Swapchain(const GPUSurfaceDescriptor& desc) : desc(desc)
{
    dxformat = infer_texture_format(format);

    recreate();

    uint image_frame_count = static_cast<uint>(frames.size());
    uint logic_frame_count = static_cast<uint>(desc.frames);

    // initialize swap frame
    assert(frames.size() == 0 || frames.size() == desc.frames);
    for (uint i = 0; i < desc.frames; i++)
        frames.at(i).init(this, i, extent.width, extent.height);

    // create image available semaphores
    uint existing_image_available_fences = static_cast<uint>(image_available_fences.size());
    assert(existing_image_available_fences == 0u || existing_image_available_fences == logic_frame_count);
    if (existing_image_available_fences == 0u) {
        image_available_fences.resize(logic_frame_count);
        for (uint i = 0; i < logic_frame_count; i++)
            api::create_fence(image_available_fences.at(i));
    }

    // create render complete semaphores (each image must have its own render complete semaphore)
    uint existing_render_complete_fences = static_cast<uint>(render_complete_fences.size());
    assert(existing_render_complete_fences == 0u || existing_render_complete_fences == image_frame_count);
    if (existing_render_complete_fences == 0u) {
        render_complete_fences.resize(image_frame_count);
        for (uint i = 0; i < image_frame_count; i++)
            api::create_fence(render_complete_fences.at(i));
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

void D3D12Swapchain::recreate()
{
    auto rhi = get_rhi();

    // query framebuffer scale
    float fb_xscale, fb_yscale;
    Window::api()->get_framebuffer_scale(desc.window, fb_xscale, fb_yscale);

    // query window size
    uint width, height;
    Window::api()->get_window_size(desc.window, width, height);
    extent.width  = static_cast<uint>(width * fb_xscale);
    extent.height = static_cast<uint>(height * fb_yscale);

    // query number of backbuffers
    uint num_backbuffers = desc.frames;

    // present mode
    present_mode = infer_present_mode(desc.present_mode);

    // check if swapchain creation if necessary
    if (swapchain != nullptr) {
        // re-create render target attachments from swapchain
        swapchain->ResizeBuffers(num_backbuffers, width, height, dxformat, 0);
    } else {
        // create swapchain
        DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
        swapchain_desc.Width                 = width;
        swapchain_desc.Height                = height;
        swapchain_desc.Format                = dxformat;
        swapchain_desc.Stereo                = false;
        swapchain_desc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT; // D3D12 disallows UAV access on back buffer textures (no direct compute shader write)
        swapchain_desc.BufferCount           = num_backbuffers;
        swapchain_desc.AlphaMode             = d3d12enum(desc.alpha_mode);
        swapchain_desc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapchain_desc.SampleDesc.Count      = 1;
        swapchain_desc.SampleDesc.Quality    = 0;

        IDXGISwapChain1* swapchain1;
        ThrowIfFailed(rhi->factory->CreateSwapChainForHwnd(rhi->graphics_queue, (HWND)desc.window.native, &swapchain_desc, nullptr, nullptr, &swapchain1));

        IDXGISwapChain3* swapchain3;
        ThrowIfFailed(swapchain1->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain3));

        swapchain = (IDXGISwapChain3*)swapchain3;
        swapchain1->Release();
    }

    // create frames if not already done so
    uint existing_frames_count = static_cast<uint>(frames.size());
    assert(frames.size() == 0 || frames.size() == num_backbuffers);
    frames.resize(num_backbuffers);
    for (uint i = existing_frames_count; i < desc.frames; i++)
        frames.at(i).init(this, i, extent.width, extent.height);
}

void D3D12Swapchain::destroy()
{
    // destroy frames
    for (auto& frame : frames)
        frame.destroy();

    // destroy semaphores
    for (auto& semaphore : image_available_fences)
        api::delete_fence(semaphore);

    // destroy semaphores
    for (auto& semaphore : render_complete_fences)
        api::delete_fence(semaphore);

    // destroy swapchain
    if (swapchain != nullptr) {
        swapchain->Release();
        swapchain = nullptr;
    }

    frames.clear();
    image_available_fences.clear();
    render_complete_fences.clear();
}
#pragma endregion D3D12Swapchain

#pragma region D3D12SwapchainFrame
void D3D12Swapchain::Frame::init(D3D12Swapchain* swp, uint backbuffer_index, uint width, uint height)
{
    auto rhi = get_rhi();

    // create a name for this swapchain texture
    auto name = std::wstring(L"Swapchain Texture ") + std::to_wstring(backbuffer_index);

    // destroy existing handles
    destroy();

    // create texture
    D3D12Texture texture;
    texture.samples     = 1;
    texture.format      = infer_texture_format(swp->format);
    texture.area.width  = width;
    texture.area.height = height;
    texture.usages      = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::COPY_DST | GPUTextureUsage::COPY_DST;
    ThrowIfFailed(swp->swapchain->GetBuffer(backbuffer_index, IID_PPV_ARGS(&texture.texture)));
    texture.texture->SetName(name.c_str());

    // create texture view
    GPUTextureViewDescriptor view_desc = {};
    view_desc.format                   = swp->format;
    view_desc.dimension                = GPUTextureViewDimension::x2D;
    view_desc.aspect                   = GPUTextureAspect::COLOR;
    view_desc.array_layer_count        = 1;
    view_desc.base_array_layer         = 0;
    view_desc.base_mip_level           = 0;
    view_desc.mip_level_count          = 1;
    view_desc.usage                    = 0;
    view_desc.label                    = "swapchain-view";
    D3D12TextureView view(texture, view_desc);

    // fill in swap frame
    this->texture = GPUTextureHandle(rhi->textures.add(texture));
    this->view    = GPUTextureViewHandle(rhi->views.add(view));
}

void D3D12Swapchain::Frame::destroy()
{
    auto rhi = get_rhi();

    // clean up existing texture
    if (texture.valid()) {
        fetch_resource(rhi->textures, texture).destroy();
        rhi->textures.remove(texture.value);
    }

    // clean up existing texture view
    if (view.valid()) {
        fetch_resource(rhi->views, view).destroy();
        rhi->views.remove(view.value);
    }
}
#pragma endregion D3D12SwapchainFrame

void default_swapchain_image_barrier(const D3D12Swapchain& swp, ID3D12GraphicsCommandList* command_buffer)
{
    auto rhi = get_rhi();

    auto& frame   = swp.frames.at(rhi->current_image_index);
    auto& texture = fetch_resource(rhi->textures, frame.texture);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = texture.texture;

    command_buffer->ResourceBarrier(1, &barrier);
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

    // backbuffer
    uint  backbuffer_index   = swp.swapchain->GetCurrentBackBufferIndex();
    auto& backbuffer         = swp.frames.at(backbuffer_index);
    rhi->current_image_index = backbuffer_index;

    // initialize suboptimal
    suboptimal = false;

    // image available from swapchain frame
    image_available_fence = swp.image_available_fences.at(ind);
    render_complete_fence = swp.render_complete_fences.at(rhi->current_image_index);

    // render complete from current frame
    auto& current_frame                 = rhi->current_frame();
    current_frame.frame_id              = rhi->current_frame_index;
    current_frame.render_complete_fence = render_complete_fence;
    current_frame.image_available_fence = image_available_fence;

    // keep track of the render complete fences
    current_frame.existing_fences.push_back(render_complete_fence);

    // update swapchain view (this must be done after current_image_index is updated)
    texture = backbuffer.texture;
    view    = backbuffer.view;

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

    // query the fences
    auto& image_available_fence = fetch_resource(rhi->fences, frame.image_available_fence);
    auto& render_complete_fence = fetch_resource(rhi->fences, frame.render_complete_fence);

    // check if nothing has been down, insert dummy workloads if needed
    if (frame.allocated_command_buffers.empty()) {
        auto  command_buffer_handle = frame.allocate(GPUQueueType::COMPUTE, true);
        auto& command_buffer        = frame.command(command_buffer_handle);
        command_buffer.begin();
        default_swapchain_image_barrier(swp, command_buffer.command_buffer);
        command_buffer.end();
        command_buffer.wait(image_available_fence, GPUBarrierSync::NONE);
        command_buffer.signal(render_complete_fence, GPUBarrierSync::ALL);
        command_buffer.submit();
    }

    // signal the image available for current frame
    auto& fence = render_complete_fence;
    rhi->graphics_queue->Signal(fence.fence, fence.target);

    // present to swapchain
    swp.swapchain->Present(swp.present_mode.sync_interval, swp.present_mode.sync_flags);
    return true;
}
