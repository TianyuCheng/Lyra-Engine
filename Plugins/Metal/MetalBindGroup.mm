#include <new>
#include <memory>
#include "MetalUtils.h"

using namespace lyra;

MetalBindGroup::MetalBindGroup() {}

void MetalBindGroup::destroy() {
    argument_buffer = nil;
    used_resources.clear();
}

MetalBindGroupHeap::MetalBindGroupHeap()
{
    arena = std::make_shared<lyra::MemoryArena>(64 * 1024); // default 64KB page size
}

MetalBindGroupHeap::MetalBindGroupHeap(const GPUBindGroupHeapDescriptor& desc)
{
    arena = std::make_shared<lyra::MemoryArena>(desc.page_size > 0 ? desc.page_size : 64 * 1024);
}

void* MetalBindGroupHeap::allocate(size_t size, size_t alignment)
{
    return arena->allocate(size, alignment);
}

GPUBindGroupHandle MetalBindGroupHeap::create_bind_group(const GPUBindGroupDescriptor& desc)
{
    auto rhi = get_rhi();
    auto& layout = fetch_resource(rhi->bind_group_layouts, desc.layout);

    if (layout.is_argument_buffer) {
        // Argument Buffer path
        void* memory = allocate(sizeof(MetalBindGroup), alignof(MetalBindGroup));
        if (!memory) {
            get_logger()->error("Failed to allocate memory for bind group from arena");
            throw GPUOutOfMemoryError("Failed to allocate memory for bind group from arena");
        }
        MetalBindGroup* group = new (memory) MetalBindGroup();
        group->heap = desc.heap;
        group->layout_handle = desc.layout;
        group->entry_count = 0;
        group->entries = nullptr;

        group->argument_buffer = [rhi->device newBufferWithLength:layout.arg_encoder.encodedLength options:MTLResourceStorageModeShared];
        if (!group->argument_buffer) {
            throw GPUOutOfMemoryError("Failed to create argument buffer");
        }
        
        [layout.arg_encoder setArgumentBuffer:group->argument_buffer offset:0];

        for (const auto& entry : desc.entries) {
            const GPUBindGroupLayoutEntry* layout_entry = nullptr;
            for (const auto& le : layout.entries) {
                if (le.binding.index == entry.binding) {
                    layout_entry = &le;
                    break;
                }
            }
            if (!layout_entry) continue;
            
            MTLResourceUsage usage = MTLResourceUsageRead;
            if (layout_entry->type == GPUResourceType::BUFFER) {
                if (layout_entry->buffer.type == GPUBufferBindingType::STORAGE) {
                    usage = MTLResourceUsageRead | MTLResourceUsageWrite;
                }
            } else if (layout_entry->type == GPUResourceType::STORAGE_TEXTURE) {
                if (layout_entry->storage_texture.access == GPUStorageTextureAccess::WRITE_ONLY) {
                    usage = MTLResourceUsageWrite;
                } else {
                    usage = MTLResourceUsageRead;
                    if (layout_entry->storage_texture.access == GPUStorageTextureAccess::READ_WRITE) {
                        usage |= MTLResourceUsageWrite;
                    }
                }
            }

            switch (entry.type) {
                case GPUResourceType::BUFFER: {
                    auto& buffer = fetch_resource(rhi->buffers, entry.buffer.buffer);
                    [layout.arg_encoder setBuffer:buffer.buffer offset:entry.buffer.offset atIndex:entry.binding];
                    group->used_resources.emplace_back(buffer.buffer, usage);
                    break;
                }
                case GPUResourceType::SAMPLER: {
                    auto& sampler = fetch_resource(rhi->samplers, entry.sampler);
                    [layout.arg_encoder setSamplerState:sampler.sampler atIndex:entry.binding];
                    // Samplers are not MTLResource
                    break;
                }
                case GPUResourceType::TEXTURE:
                case GPUResourceType::STORAGE_TEXTURE: {
                    auto& view = fetch_resource(rhi->views, entry.texture);
                    [layout.arg_encoder setTexture:view.texture atIndex:entry.binding];
                    group->used_resources.emplace_back(view.texture, usage);
                    break;
                }
                case GPUResourceType::ACCELERATION_STRUCTURE: {
                    if (!entry.tlas.valid()) {
                        throw GPUValidationError("Invalid TLAS handle for acceleration structure binding");
                    }
                    auto& tlas = fetch_resource(rhi->tlases, entry.tlas);
                    [layout.arg_encoder setAccelerationStructure:tlas.tlas atIndex:entry.binding];
                    group->used_resources.emplace_back(tlas.tlas, usage);
                    break;
                }
            }
        }
        return GPUBindGroupHandle(reinterpret_cast<uint64_t>(group));
    } else {
        // Direct binding path
        uint32_t entry_count = static_cast<uint32_t>(desc.entries.size());
        size_t   total_size  = sizeof(MetalBindGroup) + entry_count * sizeof(MetalBindGroup::Entry);

        void* memory = allocate(total_size, alignof(MetalBindGroup));
        if (!memory) {
            get_logger()->error("Failed to allocate memory for bind group from arena");
            throw GPUOutOfMemoryError("Failed to allocate memory for bind group from arena");
        }

        MetalBindGroup* group = new (memory) MetalBindGroup();
        group->heap = desc.heap;
        group->layout_handle = desc.layout;
        group->entry_count = entry_count;
        group->entries = reinterpret_cast<MetalBindGroup::Entry*>(static_cast<uint8_t*>(memory) + sizeof(MetalBindGroup));
        
        for (uint32_t i = 0; i < group->entry_count; ++i) {
            const auto& src = desc.entries[i];
            auto&       dst = group->entries[i];

            dst.binding = src.binding;
            dst.type    = src.type;

            switch (src.type) {
                case GPUResourceType::BUFFER:
                {
                    auto& buffer      = fetch_resource(rhi->buffers, src.buffer.buffer);
                    dst.buffer.buffer = buffer.buffer;
                    dst.buffer.offset = src.buffer.offset;
                    break;
                }
                case GPUResourceType::SAMPLER:
                {
                    auto& sampler       = fetch_resource(rhi->samplers, src.sampler);
                    dst.sampler.sampler = sampler.sampler;
                    break;
                }
                case GPUResourceType::TEXTURE:
                case GPUResourceType::STORAGE_TEXTURE:
                {
                    auto& view          = fetch_resource(rhi->views, src.texture);
                    dst.texture.texture = view.texture;
                    break;
                }
                case GPUResourceType::ACCELERATION_STRUCTURE:
                {
                    if (!src.tlas.valid()) {
                        throw GPUValidationError("Invalid TLAS handle for acceleration structure binding");
                    }
                    auto& tlas    = fetch_resource(rhi->tlases, src.tlas);
                    dst.tlas.tlas = tlas.tlas;
                    break;
                }
            }
        }
        return GPUBindGroupHandle(reinterpret_cast<uint64_t>(group));
    }
}

void MetalBindGroupHeap::reset()
{
    arena->reset();
}

void MetalBindGroupHeap::destroy()
{
    arena->destroy();
}

bool api::create_bind_group_heap(GPUBindGroupHeapHandle& handle, const GPUBindGroupHeapDescriptor& desc)
{
    auto rhi = get_rhi();
    auto ind = rhi->bind_group_heaps.add(MetalBindGroupHeap(desc));
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
