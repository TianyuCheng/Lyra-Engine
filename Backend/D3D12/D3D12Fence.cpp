// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

D3D12Fence::D3D12Fence()
{
    // do nothing
}

D3D12Fence::D3D12Fence(bool signaled)
{
    init(signaled);
}

void D3D12Fence::init(bool signaled)
{
    destroy();

    auto rhi = get_rhi();
    ThrowIfFailed(rhi->device->CreateFence(signaled ? 1 : 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    // create an event to allow it to be waitable on CPU
    event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void D3D12Fence::wait(uint64_t timeout)
{
    if (fence->GetCompletedValue() < target) {
        fence->SetEventOnCompletion(target, event);
        WaitForSingleObject(event, static_cast<DWORD>(timeout));
    }
}

void D3D12Fence::reset()
{
    target = fence->GetCompletedValue() + 1;
}

bool D3D12Fence::ready()
{
    return fence->GetCompletedValue() >= target;
}

void D3D12Fence::signal(ID3D12CommandQueue* queue, uint64_t value)
{
    queue->Signal(fence, value);
}

void D3D12Fence::destroy()
{
    if (fence) {
        fence->Release();
        fence = nullptr;

        CloseHandle(event);
    }
}
