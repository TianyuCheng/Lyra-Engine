#include "MetalUtils.h"
using namespace lyra;

MetalBindGroupHeap::MetalBindGroupHeap() {}

MetalBindGroupHeap::MetalBindGroupHeap(const GPUBindGroupHeapDescriptor& desc)
{
    auto rhi = get_rhi();

    // Create heap descriptor for argument buffers
    descriptor = [MTLHeapDescriptor new];

    // Size based on page_size * typical argument buffer size (256 bytes each is a reasonable estimate)
    NSUInteger heap_size = desc.page_size * 256;
    descriptor.size = heap_size;
    descriptor.storageMode = MTLStorageModeShared;  // CPU and GPU accessible
    descriptor.cpuCacheMode = MTLCPUCacheModeWriteCombined;
    descriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    descriptor.type = MTLHeapTypeAutomatic;

    heap = [rhi->device newHeapWithDescriptor:descriptor];
    if (!heap) {
        get_logger()->error("Failed to create bind group heap");
        return;
    }

    heap_offset = 0;
}

void MetalBindGroupHeap::destroy()
{
    for (auto& buffer : allocated_buffers) {
        buffer = nil;
    }
    allocated_buffers.clear();
    heap = nil;
    descriptor = nil;
    heap_offset = 0;
}

void MetalBindGroupHeap::reset()
{
    // Release allocated buffers but keep heap
    for (auto& buffer : allocated_buffers) {
        buffer = nil;
    }
    allocated_buffers.clear();
    heap_offset = 0;
}

id<MTLBuffer> MetalBindGroupHeap::allocate(GPUBindGroupLayoutHandle layout_handle, const GPUBindGroupDescriptor& desc)
{
    auto rhi = get_rhi();
    auto& layout = fetch_resource(rhi->bind_group_layouts, layout_handle);

    if (!layout.encoder) {
        get_logger()->error("Bind group layout has no argument encoder");
        return nil;
    }

    // Get required size from layout encoder
    NSUInteger required_size = layout.encoded_length;
    if (required_size == 0) {
        required_size = 256;  // Minimum allocation
    }

    // Align to 256 bytes (Metal requirement for argument buffers)
    required_size = (required_size + 255) & ~255;

    // Allocate buffer from heap
    id<MTLBuffer> arg_buffer = [heap newBufferWithLength:required_size
                                                 options:MTLResourceStorageModeShared];
    if (!arg_buffer) {
        get_logger()->error("Failed to allocate argument buffer from heap");
        return nil;
    }

    // Encode resources into argument buffer
    [layout.encoder setArgumentBuffer:arg_buffer offset:0];

    for (auto& entry : desc.entries) {
        switch (entry.type) {
            case GPUBindingResourceType::BUFFER:
            {
                auto& buffer = fetch_resource(rhi->buffers, entry.buffer.buffer);
                [layout.encoder setBuffer:buffer.buffer
                                   offset:entry.buffer.offset
                                  atIndex:entry.binding];
                break;
            }
            case GPUBindingResourceType::SAMPLER:
            {
                auto& sampler = fetch_resource(rhi->samplers, entry.sampler);
                [layout.encoder setSamplerState:sampler.sampler
                                        atIndex:entry.binding];
                break;
            }
            case GPUBindingResourceType::TEXTURE:
            case GPUBindingResourceType::STORAGE_TEXTURE:
            {
                auto& texture_view = fetch_resource(rhi->views, entry.texture);
                [layout.encoder setTexture:texture_view.texture
                                   atIndex:entry.binding];
                break;
            }
            case GPUBindingResourceType::ACCELERATION_STRUCTURE:
            {
                // Metal 3+ acceleration structure binding
                if (![rhi->device supportsRaytracing]) {
                    get_logger()->error("Cannot bind acceleration structure: ray tracing not supported");
                    break;
                }

                if (!entry.tlas.valid()) {
                    get_logger()->error("Invalid TLAS handle for acceleration structure binding");
                    break;
                }

                auto& tlas = fetch_resource(rhi->tlases, entry.tlas);
                if (!tlas.tlas) {
                    get_logger()->error("TLAS acceleration structure is null");
                    break;
                }

                // Metal requires acceleration structures to be set via setAccelerationStructure:atIndex:
                // which is available on MTLArgumentEncoder in Metal 3+
                if (@available(macOS 13.0, iOS 16.0, *)) {
                    [layout.encoder setAccelerationStructure:tlas.tlas atIndex:entry.binding];
                } else {
                    get_logger()->error("Acceleration structure binding requires macOS 13.0+ or iOS 16.0+");
                }
                break;
            }
        }
    }

    allocated_buffers.push_back(arg_buffer);
    heap_offset += required_size;

    return arg_buffer;
}

bool api::create_bind_group(GPUBindGroupHandle& handle, const GPUBindGroupDescriptor& desc)
{
    if (!desc.heap.valid()) {
        get_logger()->error("Failed to create bind group: heap is invalid");
        return false;
    }

    auto rhi = get_rhi();
    auto& heap = fetch_resource(rhi->bind_group_heaps, desc.heap);

    // Allocate argument buffer from heap
    id<MTLBuffer> arg_buffer = heap.allocate(desc.layout, desc);
    if (!arg_buffer) {
        return false;
    }

    // Store the argument buffer pointer as the handle value
    // This allows direct access without indirection through a resource manager
    // The buffer is owned by the heap and will be cleaned up when heap is reset/destroyed
    handle = GPUBindGroupHandle(reinterpret_cast<uint64_t>((__bridge void*)arg_buffer));
    return true;
}

bool api::create_bind_group_heap(GPUBindGroupHeapHandle& handle, const GPUBindGroupHeapDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalBindGroupHeap(desc);
    if (!obj.valid()) {
        return false;
    }
    auto ind = rhi->bind_group_heaps.add(obj);
    handle = GPUBindGroupHeapHandle(ind);
    return true;
}

void api::delete_bind_group_heap(GPUBindGroupHeapHandle handle)
{
    get_rhi()->bind_group_heaps.remove(handle.value);
}

void api::reset_bind_group_heap(GPUBindGroupHeapHandle handle)
{
    auto rhi = get_rhi();
    auto& heap = fetch_resource(rhi->bind_group_heaps, handle);
    heap.reset();
}
