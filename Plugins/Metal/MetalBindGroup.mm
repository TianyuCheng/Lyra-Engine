#include <new>
#include <memory>
#include "MetalUtils.h"

using namespace lyra;

MetalBindGroup::MetalBindGroup() {}

void MetalBindGroup::init(const GPUBindGroupDescriptor& desc)
{
    auto rhi = get_rhi();

    for (uint32_t i = 0; i < entry_count; ++i) {
        const auto& src = desc.entries[i];
        auto&       dst = entries[i];

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
                    get_logger()->error("Invalid TLAS handle for acceleration structure binding");
                    break;
                }
                auto& tlas    = fetch_resource(rhi->tlases, src.tlas);
                dst.tlas.tlas = tlas.tlas;
                break;
            }
        }
    }
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
    uint32_t entry_count = static_cast<uint32_t>(desc.entries.size());
    size_t   total_size  = sizeof(MetalBindGroup) + entry_count * sizeof(MetalBindGroup::Entry);

    void* memory = allocate(total_size, alignof(MetalBindGroup));
    if (!memory) {
        get_logger()->error("Failed to allocate memory for bind group from arena");
        return GPUBindGroupHandle(0);
    }

    // construct and initialize
    MetalBindGroup* group = new (memory) MetalBindGroup();
    group->heap           = desc.heap;
    group->entry_count    = entry_count;
    group->entries        = reinterpret_cast<MetalBindGroup::Entry*>(static_cast<uint8_t*>(memory) + sizeof(MetalBindGroup));
    group->init(desc);
    return GPUBindGroupHandle(reinterpret_cast<uint64_t>(group));
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
