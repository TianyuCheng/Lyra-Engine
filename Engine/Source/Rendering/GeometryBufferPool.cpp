#include <algorithm>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Graphics/RHIUtils.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/GeometryBufferPool.h>

using namespace lyra;

GeometryBufferPool::GeometryBufferPool(ResourceRetire& resource_retire, GPUSize64 slab_size)
    : resource_retire(resource_retire), default_slab_size(slab_size)
{
    // do nothing else
}

GeometryBufferPool::~GeometryBufferPool()
{
    clear();
}

BufferSlice GeometryBufferPool::allocate(GPUSize64 size, GPUSize64 alignment)
{
    if (size == 0) {
        return BufferSlice{};
    }

    if (alignment == 0) {
        alignment = 1;
    }

    // 1. try allocating from existing non-dedicated slabs
    for (auto& slab : slabs) {
        if (slab.is_dedicated) {
            continue;
        }

        if (auto slice = try_allocate_from_slab(slab, size, alignment); slice.has_value()) {
            return *slice;
        }
    }

    // 2. no slab had space: create a new slab
    bool  dedicated     = size > default_slab_size;
    auto  slab_capacity = dedicated ? size : default_slab_size;
    auto& new_slab      = create_slab(slab_capacity, dedicated);

    // allocate from new slab
    GPUSize64 aligned_offset = 0;
    GPUSize64 end_offset     = size;
    GPUSize64 back_rem       = new_slab.capacity - end_offset;

    new_slab.free_blocks.clear();
    if (back_rem > 0) {
        FreeBlock trailing_block;
        trailing_block.offset = end_offset;
        trailing_block.size   = back_rem;
        new_slab.free_blocks.push_back(trailing_block);
    }

    new_slab.allocated_bytes += size;
    used_bytes += size;

    BufferSlice slice;
    slice.buffer  = new_slab.buffer;
    slice.offset  = aligned_offset;
    slice.size    = size;
    slice.slab_id = new_slab.id;
    return slice;
}

void GeometryBufferPool::free(const BufferSlice& slice)
{
    if (!slice.valid()) {
        return;
    }

    for (auto it = slabs.begin(); it != slabs.end(); ++it) {
        if (it->id != slice.slab_id) {
            continue;
        }

        if (it->is_dedicated) {
            used_bytes -= slice.size;
            total_capacity -= it->capacity;
            destroy_buffer(it->buffer);
            slabs.erase(it);
            return;
        }

        // return block
        FreeBlock freed;
        freed.offset = slice.offset;
        freed.size   = slice.size;
        it->free_blocks.push_back(freed);

        it->allocated_bytes = (it->allocated_bytes >= slice.size) ? (it->allocated_bytes - slice.size) : 0;
        used_bytes          = (used_bytes >= slice.size) ? (used_bytes - slice.size) : 0;

        coalesce_free_blocks(*it);

        // if slab is completely empty and we have extra slabs, reclaim it
        if (it->allocated_bytes == 0 && slabs.size() > 1) {
            total_capacity -= it->capacity;
            destroy_buffer(it->buffer);
            slabs.erase(it);
        }
        return;
    }
}

void GeometryBufferPool::clear()
{
    for (auto& slab : slabs) {
        destroy_buffer(slab.buffer);
    }
    slabs.clear();
    total_capacity = 0;
    used_bytes     = 0;
}

GeometryBufferPool::Slab& GeometryBufferPool::create_slab(GPUSize64 capacity, bool dedicated)
{
    Slab slab;
    slab.id           = next_slab_id++;
    slab.capacity     = capacity;
    slab.is_dedicated = dedicated;

    auto desc  = GPUBufferDescriptor{};
    desc.label = dedicated ? "geometry_dedicated_slab" : "geometry_pool_slab";
    desc.size  = capacity;
    desc.usage = GPUBufferUsage::VERTEX | GPUBufferUsage::INDEX | GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST;

    if (RHI::api()) {
        auto& device = RHI::get_current_device();
        slab.buffer  = device.create_buffer(desc);
    }

    total_capacity += capacity;

    FreeBlock initial_block;
    initial_block.offset = 0;
    initial_block.size   = capacity;
    slab.free_blocks.push_back(initial_block);

    slabs.push_back(std::move(slab));
    return slabs.back();
}

void GeometryBufferPool::destroy_buffer(GPUBuffer& buffer)
{
    if (!buffer.valid()) {
        return;
    }

    resource_retire.retire(buffer);
}

Optional<BufferSlice> GeometryBufferPool::try_allocate_from_slab(Slab& slab, GPUSize64 size, GPUSize64 alignment)
{
    for (usize i = 0; i < slab.free_blocks.size(); ++i) {
        auto&     block          = slab.free_blocks[i];
        GPUSize64 aligned_offset = (block.offset + alignment - 1) & ~(alignment - 1);
        GPUSize64 end_offset     = aligned_offset + size;

        if (end_offset > block.offset + block.size) {
            continue;
        }

        GPUSize64 front_gap = aligned_offset - block.offset;
        GPUSize64 back_rem  = (block.offset + block.size) - end_offset;

        uint slab_id = slab.id;
        auto buffer  = slab.buffer;

        // replace or split block
        if (front_gap > 0 && back_rem > 0) {
            block.size = front_gap;
            FreeBlock trailing_block;
            trailing_block.offset = end_offset;
            trailing_block.size   = back_rem;
            slab.free_blocks.insert(slab.free_blocks.begin() + i + 1, trailing_block);
        } else if (front_gap > 0) {
            block.size = front_gap;
        } else if (back_rem > 0) {
            block.offset = end_offset;
            block.size   = back_rem;
        } else {
            slab.free_blocks.erase(slab.free_blocks.begin() + i);
        }

        slab.allocated_bytes += size;
        used_bytes += size;

        BufferSlice slice;
        slice.buffer  = buffer;
        slice.offset  = aligned_offset;
        slice.size    = size;
        slice.slab_id = slab_id;
        return slice;
    }

    return std::nullopt;
}

void GeometryBufferPool::coalesce_free_blocks(Slab& slab)
{
    std::sort(slab.free_blocks.begin(), slab.free_blocks.end(), [](const FreeBlock& a, const FreeBlock& b) {
        return a.offset < b.offset;
    });

    for (usize i = 0; i + 1 < slab.free_blocks.size();) {
        if (slab.free_blocks[i].offset + slab.free_blocks[i].size == slab.free_blocks[i + 1].offset) {
            slab.free_blocks[i].size += slab.free_blocks[i + 1].size;
            slab.free_blocks.erase(slab.free_blocks.begin() + i + 1);
        } else {
            ++i;
        }
    }
}
