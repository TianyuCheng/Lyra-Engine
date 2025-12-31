#include "MetalUtils.h"

using namespace lyra;

MetalBindGroup::MetalBindGroup()
{
    // do nothing
}

MetalBindGroup::MetalBindGroup(const GPUBindGroupDescriptor& desc)
{
    auto rhi = get_rhi();

    for (const auto& entry : desc.entries) {
        Entry new_entry;
        new_entry.binding = entry.binding;
        new_entry.type    = entry.type;

        switch (entry.type) {
            case GPUResourceType::BUFFER:
            {
                auto& buffer            = fetch_resource(rhi->buffers, entry.buffer.buffer);
                new_entry.buffer.buffer = buffer.buffer;
                new_entry.buffer.offset = entry.buffer.offset;
                break;
            }
            case GPUResourceType::SAMPLER:
            {
                auto& sampler             = fetch_resource(rhi->samplers, entry.sampler);
                new_entry.sampler.sampler = sampler.sampler;
                break;
            }
            case GPUResourceType::TEXTURE:
            case GPUResourceType::STORAGE_TEXTURE:
            {
                auto& view                = fetch_resource(rhi->views, entry.texture);
                new_entry.texture.texture = view.texture;
                break;
            }
            case GPUResourceType::ACCELERATION_STRUCTURE:
            {
                if (!entry.tlas.valid()) {
                    get_logger()->error("Invalid TLAS handle");
                    break;
                }
                auto& tlas          = fetch_resource(rhi->tlases, entry.tlas);
                new_entry.tlas.tlas = tlas.tlas;
                break;
            }
        }
        entries.push_back(new_entry);
    }
}

MetalBindGroupHeap::MetalBindGroupHeap()
{
    // do nothing
}

MetalBindGroupHeap::MetalBindGroupHeap(const GPUBindGroupHeapDescriptor& desc)
{
    // reserve if max_bind_groups is provided?
    // desc.max_bind_groups
}

uint32_t MetalBindGroupHeap::allocate(const GPUBindGroupDescriptor& desc)
{
    groups.emplace_back(desc);
    return static_cast<uint32_t>(groups.size() - 1);
}

void MetalBindGroupHeap::reset()
{
    groups.clear();
}

void MetalBindGroupHeap::destroy()
{
    groups.clear();
}

bool api::create_bind_group(GPUBindGroupHandle& handle, const GPUBindGroupDescriptor& desc)
{
    auto rhi = get_rhi();

    if (!desc.heap.valid()) {
        get_logger()->error("Creating bind group requires a valid heap handle.");
        return false;
    }

    auto& heap = fetch_resource(rhi->bind_group_heaps, desc.heap);

    uint32_t index = heap.allocate(desc);

    // construct handle: (HeapID << 32) | GroupIndex
    // assuming heap.value is the index in the resource manager and fits in 32 bits
    uint64_t heap_id      = desc.heap.value;
    uint64_t handle_value = (heap_id << 32) | index;

    handle = GPUBindGroupHandle(handle_value);
    return true;
}

bool api::create_bind_group_heap(GPUBindGroupHeapHandle& handle, const GPUBindGroupHeapDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalBindGroupHeap(desc);
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
