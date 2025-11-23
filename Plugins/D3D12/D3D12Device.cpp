// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

bool api::create_device(const GPUDeviceDescriptor& desc)
{
    auto rhi = get_rhi();

    // create device
    ThrowIfFailed(D3D12CreateDevice(rhi->adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&rhi->device)));

    // fetch debug device
    if (rhi->rhiflags.contains(RHIFlag::DEBUG))
        ThrowIfFailed(rhi->device->QueryInterface(&rhi->debug_device));

    // create allocator
    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice                 = rhi->device;
    allocatorDesc.pAdapter                = rhi->adapter;
    allocatorDesc.Flags                   = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;
    ThrowIfFailed(D3D12MA::CreateAllocator(&allocatorDesc, &rhi->allocator));

    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask                 = 0; // usually 0 for single GPU

    // create graphics queue
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(rhi->device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&rhi->graphics_queue)));

    // create compute queue
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    ThrowIfFailed(rhi->device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&rhi->compute_queue)));

    // create transfer queue
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    ThrowIfFailed(rhi->device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&rhi->transfer_queue)));

    // create fence
    rhi->idle_fence.init(false);
    rhi->idle_fence.fence->SetName(L"idle fence");

    // create cpu descriptor heaps
    rhi->rtv_heap.init(32, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    rhi->dsv_heap.init(32, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    rhi->sampler_heap.init(32, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    rhi->cbv_srv_uav_heap.init(512, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    // create gpu descriptor heap allocators
    rhi->gpu_default_heap.init(4096, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    rhi->gpu_sampler_heap.init(2048, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    // create a default frame (for headless cases)
    rhi->frames.emplace_back();
    rhi->frames.back().init();

    return true;
}

void api::delete_device()
{
    api::wait_idle();

    auto rhi = get_rhi();

    // clean up remaining frames
    for (auto& frame : rhi->frames)
        frame.destroy();

    // clean up remaining swapchains
    for (auto& swapchain : rhi->swapchains)
        if (swapchain.valid())
            swapchain.destroy();

    // clean up remaining blases
    for (auto& blas : rhi->blases)
        if (blas.valid())
            blas.destroy();

    // clean up remaining tlases
    for (auto& tlas : rhi->tlases)
        if (tlas.valid())
            tlas.destroy();

    // clean up remaining fences
    for (auto& fence : rhi->fences)
        if (fence.valid())
            fence.destroy();

    // clean up remaining buffers
    for (auto& buffer : rhi->buffers)
        if (buffer.valid())
            buffer.destroy();

    // clean up remaining texture views
    for (auto& view : rhi->views)
        if (view.valid())
            view.destroy();

    // clean up remaining textures
    for (auto& texture : rhi->textures)
        if (texture.valid())
            texture.destroy();

    // clean up remaining samplers
    for (auto& sampler : rhi->samplers)
        if (sampler.valid())
            sampler.destroy();

    // clean up remaining shaders
    for (auto& shader : rhi->shaders)
        if (shader.valid())
            shader.destroy();

    // clean up remaining bind group heaps
    for (auto& heap : rhi->bind_group_heaps)
        if (heap.valid())
            heap.destroy();

    // clean up remaining bind group layouts
    for (auto& layout : rhi->bind_group_layouts)
        if (layout.valid())
            layout.destroy();

    // clean up remaining pipeline layouts
    for (auto& layout : rhi->pipeline_layouts)
        if (layout.valid())
            layout.destroy();

    // clean up remaining pipelines
    for (auto& pipeline : rhi->pipelines)
        if (pipeline.valid())
            pipeline.destroy();

    // clean up gpu descriptor heaps
    rhi->gpu_default_heap.destroy();
    rhi->gpu_sampler_heap.destroy();

    // clean up cpu descriptor heaps
    rhi->rtv_heap.destroy();
    rhi->dsv_heap.destroy();
    rhi->sampler_heap.destroy();
    rhi->cbv_srv_uav_heap.destroy();

    if (rhi->idle_fence.valid()) {
        rhi->idle_fence.destroy();
    }

    if (rhi->transfer_queue) {
        rhi->transfer_queue->Release();
        rhi->transfer_queue = nullptr;
    }

    if (rhi->graphics_queue) {
        rhi->graphics_queue->Release();
        rhi->graphics_queue = nullptr;
    }

    if (rhi->compute_queue) {
        rhi->compute_queue->Release();
        rhi->compute_queue = nullptr;
    }

    if (rhi->allocator) {
        rhi->allocator->Release();
        rhi->allocator = nullptr;
    }

    if (rhi->device) {
        rhi->device->Release();
        rhi->device = nullptr;
    }

    if (rhi->debug_device) {
        rhi->debug_device->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        rhi->debug_device->Release();
        rhi->debug_device = nullptr;
    }
}
