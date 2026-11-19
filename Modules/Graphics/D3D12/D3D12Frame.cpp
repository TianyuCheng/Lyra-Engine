#include "D3D12Utils.h"

void D3D12Frame::init()
{
}

void D3D12Frame::wait()
{
    if (existing_fences.empty()) return;

    auto rhi = get_rhi();
    for (auto& fence : existing_fences) {
        fetch_resource(rhi->fences, fence).wait();
    }
}

void D3D12Frame::reset()
{
    // reset all fences
    if (!existing_fences.empty()) {
        auto rhi = get_rhi();
        for (auto& fence : existing_fences)
            fetch_resource(rhi->fences, fence).reset();
    }

    existing_fences.clear();
    frame_has_work = false;
}

void D3D12Frame::free()
{
    reset();
}

void D3D12Frame::destroy()
{
    reset();
}
