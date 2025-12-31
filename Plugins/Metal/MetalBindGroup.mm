#include "MetalUtils.h"
#include <new>

using namespace lyra;

MetalBindGroup::MetalBindGroup()
{
    // do nothing
}

MetalBindGroup::MetalBindGroup(const GPUBindGroupDescriptor& desc)
{
    // this constructor is not used with the arena allocator,
    // as objects are constructed in-place.
}

// wrapper around lyra::MemoryArena
MetalBindGroupHeap::MetalBindGroupHeap()
{
    // default 64KB page size
    arena = std::make_shared<lyra::MemoryArena>(64 * 1024);
}

MetalBindGroupHeap::MetalBindGroupHeap(const GPUBindGroupHeapDescriptor& desc)
{
    // the arena is constructed with the given page size.
    arena = std::make_shared<lyra::MemoryArena>(desc.page_size > 0 ? desc.page_size : 64 * 1024);
}

void* MetalBindGroupHeap::allocate(size_t size, size_t alignment)
{
    return arena->allocate(size, alignment);
}

void MetalBindGroupHeap::reset()
{
    arena->reset();
}

void MetalBindGroupHeap::destroy()
{
    arena->destroy();
}

bool api::create_bind_group(GPUBindGroupHandle& handle, const GPUBindGroupDescriptor& desc)
{
    auto rhi = get_rhi();

    if (!desc.heap.valid()) {
        get_logger()->error("Create bind group requires a valid heap handle");
        return false;
    }

    auto& heap = fetch_resource(rhi->bind_group_heaps, desc.heap);

    // layout calculation
    uint32_t entry_count = static_cast<uint32_t>(desc.entries.size());
    size_t   total_size  = sizeof(MetalBindGroup) + entry_count * sizeof(MetalBindGroup::Entry);

    // allocate memory from the arena
    void* memory = heap.allocate(total_size, alignof(MetalBindGroup));
    if (!memory) {
        get_logger()->error("Failed to allocate memory for bind group from heap");
        return false;
    }

    // construct MetalBindGroup in-place
    MetalBindGroup* group = new (memory) MetalBindGroup();
    group->heap           = desc.heap;
    group->entry_count    = entry_count;
    group->entries        = reinterpret_cast<MetalBindGroup::Entry*>(static_cast<uint8_t*>(memory) + sizeof(MetalBindGroup));

    // initialize entries
    for (uint32_t i = 0; i < entry_count; ++i) {
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
                    get_logger()->error("Invalid TLAS handle");
                    break;
                }
                auto& tlas    = fetch_resource(rhi->tlases, src.tlas);
                dst.tlas.tlas = tlas.tlas;
                break;
            }
        }
    }

    // handle is the pointer to the allocated memory
    handle = GPUBindGroupHandle(reinterpret_cast<uint64_t>(group));
    return true;
}

bool api::create_bind_group_heap(GPUBindGroupHeapHandle& handle, const GPUBindGroupHeapDescriptor& desc)
{
    auto rhi = get_rhi();
    // Pass a temporary r-value to trigger move semantics
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
