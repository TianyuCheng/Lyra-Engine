#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameArena.h>

using namespace lyra;

FrameArena::FrameArena(uint frames_in_flight, uint default_page_size)
    : frames_in_flight(frames_in_flight), page_size(default_page_size)
{
    // nothing
}

FrameArena::~FrameArena()
{
    destroy();
}

FrameArena::FrameArena(FrameArena&& other) noexcept
    : device(other.device), frames_in_flight(other.frames_in_flight), current_frame_slot(other.current_frame_slot), page_size(other.page_size), bind_group_heaps(std::move(other.bind_group_heaps))
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
        page_size          = other.page_size;
        bind_group_heaps   = std::move(other.bind_group_heaps);

        other.device             = {};
        other.frames_in_flight   = 0;
        other.current_frame_slot = 0;
    }
    return *this;
}

void FrameArena::init(GPUDevice dev, uint fif, uint ps)
{
    destroy();

    device             = dev;
    frames_in_flight   = std::max(1u, fif);
    page_size          = ps;
    current_frame_slot = 0;

    bind_group_heaps.resize(frames_in_flight);
    for (auto& heap : bind_group_heaps) {
        auto heap_desc      = GPUBindGroupHeapDescriptor{};
        heap_desc.page_size = page_size;
        heap                = device.create_bind_group_heap(heap_desc);
    }
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
    current_frame_slot = 0;
}

GPUBindGroup FrameArena::allocate(const GPUBindGroupDescriptor& descriptor, GPUDevice dev)
{
    auto target_device = dev.valid() ? dev : (device.valid() ? device : RHI::get_current_device());
    auto desc          = descriptor;

    if (!desc.heap.valid()) {
        if (bind_group_heaps.empty()) {
            init(target_device, frames_in_flight, page_size);
        }
        if (current_frame_slot < bind_group_heaps.size() && bind_group_heaps[current_frame_slot].valid()) {
            desc.heap = bind_group_heaps[current_frame_slot].handle;
        }
    }

    return target_device.create_bind_group(desc);
}

void FrameArena::tick()
{
    if (frames_in_flight > 0 && !bind_group_heaps.empty()) {
        current_frame_slot = (current_frame_slot + 1) % frames_in_flight;
        if (bind_group_heaps[current_frame_slot].valid()) {
            bind_group_heaps[current_frame_slot].reset();
        }
    }
}
