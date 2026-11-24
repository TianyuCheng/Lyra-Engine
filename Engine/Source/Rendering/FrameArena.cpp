#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameArena.h>

using namespace lyra;

FrameArena::FrameArena(uint frames_in_flight, uint default_heap_page_size, uint default_uniform_page_size)
    : frames_in_flight(frames_in_flight), heap_page_size(default_heap_page_size), uniform_page_size(default_uniform_page_size)
{
    // nothing
}

FrameArena::~FrameArena()
{
    destroy();
}

FrameArena::FrameArena(FrameArena&& other) noexcept
    : device(other.device), frames_in_flight(other.frames_in_flight), current_frame_slot(other.current_frame_slot), heap_page_size(other.heap_page_size), uniform_page_size(other.uniform_page_size), bind_group_heaps(std::move(other.bind_group_heaps)), uniform_slots(std::move(other.uniform_slots))
{
    other.device             = {};
    other.frames_in_flight   = 0;
    other.current_frame_slot = 0;
}

FrameArena& FrameArena::operator=(FrameArena&& other) noexcept
{
    if (this != &other) {
        destroy();
        device             = other.device;
        frames_in_flight   = other.frames_in_flight;
        current_frame_slot = other.current_frame_slot;
        heap_page_size     = other.heap_page_size;
        uniform_page_size  = other.uniform_page_size;
        bind_group_heaps   = std::move(other.bind_group_heaps);
        uniform_slots      = std::move(other.uniform_slots);

        other.device             = {};
        other.frames_in_flight   = 0;
        other.current_frame_slot = 0;
    }
    return *this;
}

void FrameArena::init(GPUDevice dev, uint fif, uint heap_ps, uint uniform_ps)
{
    destroy();

    device             = dev;
    frames_in_flight   = std::max(1u, fif);
    heap_page_size     = heap_ps;
    uniform_page_size  = uniform_ps;
    current_frame_slot = 0;

    bind_group_heaps.resize(frames_in_flight);
    for (auto& heap : bind_group_heaps) {
        auto heap_desc      = GPUBindGroupHeapDescriptor{};
        heap_desc.page_size = heap_page_size;
        heap                = device.create_bind_group_heap(heap_desc);
    }

    uniform_slots.resize(frames_in_flight);
}

void FrameArena::destroy()
{
    auto* api = RHI::api();
    for (auto& heap : bind_group_heaps) {
        if (heap.valid() && api) {
            heap.destroy();
        }
    }
    bind_group_heaps.clear();

    for (auto& slot : uniform_slots) {
        for (auto& page : slot.pages) {
            if (page.buffer.valid() && api) {
                page.buffer.destroy();
            }
        }
        slot.pages.clear();
    }
    uniform_slots.clear();

    current_frame_slot = 0;
}

GPUBindGroup FrameArena::allocate(const GPUBindGroupDescriptor& descriptor, GPUDevice dev)
{
    auto target_device = dev.valid() ? dev : (device.valid() ? device : RHI::get_current_device());
    auto desc          = descriptor;

    if (!desc.heap.valid()) {
        if (bind_group_heaps.empty()) {
            init(target_device, frames_in_flight, heap_page_size, uniform_page_size);
        }
        if (current_frame_slot < bind_group_heaps.size() && bind_group_heaps[current_frame_slot].valid()) {
            desc.heap = bind_group_heaps[current_frame_slot].handle;
        }
    }

    return target_device.create_bind_group(desc);
}

GPUBufferSlice FrameArena::allocate_uniform(usize size, usize alignment, GPUDevice dev)
{
    if (size == 0) {
        return {};
    }

    auto target_device = dev.valid() ? dev : (device.valid() ? device : RHI::get_current_device());

    if (uniform_slots.empty()) {
        init(target_device, frames_in_flight, heap_page_size, uniform_page_size);
    }

    if (current_frame_slot >= uniform_slots.size()) {
        return {};
    }

    auto& slot = uniform_slots[current_frame_slot];

    // ensure alignment is at least 1 and power of two
    alignment = std::max<usize>(1, alignment);

    // try allocating in existing pages for this slot
    while (slot.current_page < slot.pages.size()) {
        auto& page           = slot.pages[slot.current_page];
        usize aligned_offset = (slot.current_offset + alignment - 1) & ~(alignment - 1);

        if (aligned_offset + size <= page.capacity) {
            GPUBufferSlice slice{};
            slice.buffer        = page.buffer.handle;
            slice.offset        = aligned_offset;
            slice.size          = size;
            slice.mapped_data   = static_cast<uint8_t*>(page.mapped_ptr) + aligned_offset;
            slot.current_offset = aligned_offset + size;
            return slice;
        }

        // current page is full, advance to next page
        slot.current_page++;
        slot.current_offset = 0;
    }

    // need a new page for this slot
    usize page_alloc_size = std::max(static_cast<usize>(uniform_page_size), size + alignment);

    GPUBufferDescriptor bdesc{};
    bdesc.label              = "frame_arena_uniform_page";
    bdesc.size               = page_alloc_size;
    bdesc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
    bdesc.mapped_at_creation = true;

    auto new_buffer = target_device.create_buffer(bdesc);
    if (!new_buffer.valid()) {
        return {};
    }

    auto range = new_buffer.get_mapped_range();
    if (!range.data) {
        new_buffer.destroy();
        return {};
    }

    UniformPage new_page{};
    new_page.buffer     = new_buffer;
    new_page.mapped_ptr = const_cast<void*>(static_cast<const void*>(range.data));
    new_page.capacity   = page_alloc_size;

    slot.pages.push_back(new_page);
    slot.current_page = slot.pages.size() - 1;

    GPUBufferSlice slice{};
    slice.buffer        = new_buffer.handle;
    slice.offset        = 0;
    slice.size          = size;
    slice.mapped_data   = new_page.mapped_ptr;
    slot.current_offset = size;

    return slice;
}

uint FrameArena::uniform_page_count() const
{
    uint count = 0;
    for (const auto& slot : uniform_slots) {
        count += static_cast<uint>(slot.pages.size());
    }
    return count;
}

void FrameArena::tick()
{
    if (frames_in_flight > 0) {
        current_frame_slot = (current_frame_slot + 1) % frames_in_flight;

        if (!bind_group_heaps.empty() && current_frame_slot < bind_group_heaps.size() && bind_group_heaps[current_frame_slot].valid()) {
            bind_group_heaps[current_frame_slot].reset();
        }

        if (current_frame_slot < uniform_slots.size()) {
            uniform_slots[current_frame_slot].current_page   = 0;
            uniform_slots[current_frame_slot].current_offset = 0;
        }
    }
}
