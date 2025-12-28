#include "MetalUtils.h"
using namespace lyra;

MetalBindGroupHeap::MetalBindGroupHeap() {}

MetalBindGroupHeap::MetalBindGroupHeap(const GPUBindGroupHeapDescriptor& desc)
{
    auto rhi = get_rhi();

    has_unified_memory = rhi->device.hasUnifiedMemory;

    if (has_unified_memory) {
        // create heap descriptor for argument buffers
        descriptor = [MTLHeapDescriptor new];

        // size based on page_size * typical argument buffer size (256 bytes each is a reasonable estimate)
        NSUInteger heap_size          = desc.page_size * 256;
        descriptor.size               = heap_size;
        descriptor.storageMode        = MTLStorageModeShared; // CPU and GPU accessible
        descriptor.cpuCacheMode       = MTLCPUCacheModeWriteCombined;
        descriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
        descriptor.type               = MTLHeapTypeAutomatic;

        heap = [rhi->device newHeapWithDescriptor:descriptor];
        if (!heap) {
            get_logger()->error("Failed to create bind group heap");
            return;
        }
    }

    heap_offset = 0;
}

void MetalBindGroupHeap::destroy()
{
    for (auto& buffer : allocated_buffers)
        buffer = nil;

    allocated_buffers.clear();
    heap        = nil;
    descriptor  = nil;
    heap_offset = 0;
}

void MetalBindGroupHeap::reset()
{
    // release allocated buffers but keep heap
    for (auto& buffer : allocated_buffers)
        buffer = nil;

    allocated_buffers.clear();
    heap_offset = 0;
}

id<MTLBuffer> MetalBindGroupHeap::allocate(GPUBindGroupLayoutHandle layout_handle, const GPUBindGroupDescriptor& desc)
{
    auto  rhi    = get_rhi();
    auto& layout = fetch_resource(rhi->bind_group_layouts, layout_handle);

    if (!layout.encoder) {
        get_logger()->error("Bind group layout has no argument encoder");
        return nil;
    }

    // get required size from layout encoder
    NSUInteger required_size = layout.encoded_length;
    if (required_size == 0) {
        required_size = 256; // Minimum allocation
    }

    // align to 256 bytes (Metal requirement for argument buffers)
    required_size = (required_size + 255) & ~255;

    // allocate buffer from heap or device
    id<MTLBuffer> arg_buffer;
    if (heap) {
        arg_buffer = [heap newBufferWithLength:required_size
                                       options:MTLResourceStorageModeShared];
    } else {
        arg_buffer = [rhi->device newBufferWithLength:required_size
                                              options:MTLResourceStorageModeManaged];
    }
    if (!arg_buffer) {
        get_logger()->error("Failed to allocate argument buffer from heap");
        return nil;
    }

    // encode resources into argument buffer
    [layout.encoder setArgumentBuffer:arg_buffer offset:0];

    for (auto& entry : desc.entries) {
        switch (entry.type) {
            case GPUResourceType::BUFFER:
            {
                auto& buffer = fetch_resource(rhi->buffers, entry.buffer.buffer);
                [layout.encoder setBuffer:buffer.buffer
                                   offset:entry.buffer.offset
                                  atIndex:entry.binding];
                break;
            }
            case GPUResourceType::SAMPLER:
            {
                auto& sampler = fetch_resource(rhi->samplers, entry.sampler);
                [layout.encoder setSamplerState:sampler.sampler
                                        atIndex:entry.binding];
                break;
            }
            case GPUResourceType::TEXTURE:
            case GPUResourceType::STORAGE_TEXTURE:
            {
                auto& texture_view = fetch_resource(rhi->views, entry.texture);
                [layout.encoder setTexture:texture_view.texture
                                   atIndex:entry.binding];
                break;
            }
            case GPUResourceType::ACCELERATION_STRUCTURE:
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

    // if memory is managed, notify the driver that the buffer was modified
    if (arg_buffer.storageMode == MTLStorageModeManaged) {
        [arg_buffer didModifyRange:NSMakeRange(0, [arg_buffer length])];
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

    auto  rhi  = get_rhi();
    auto& heap = fetch_resource(rhi->bind_group_heaps, desc.heap);

    // Allocate argument buffer from heap
    id<MTLBuffer> arg_buffer = heap.allocate(desc.layout, desc);
    if (!arg_buffer) {
        return false;
    }

    // store the argument buffer pointer as the handle value,
    // this allows direct access without indirection through a resource manager,
    // the buffer is owned by the heap and will be cleaned up when heap is reset/destroyed
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
    handle   = GPUBindGroupHeapHandle(ind);
    return true;
}

void api::delete_bind_group_heap(GPUBindGroupHeapHandle handle)
{
    get_rhi()->bind_group_heaps.remove(handle.value);
}

void api::reset_bind_group_heap(GPUBindGroupHeapHandle handle)
{
    auto  rhi  = get_rhi();
    auto& heap = fetch_resource(rhi->bind_group_heaps, handle);
    heap.reset();
}
