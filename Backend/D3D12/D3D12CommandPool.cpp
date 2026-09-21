// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

void D3D12CommandPool::init(D3D12_COMMAND_LIST_TYPE type)
{
    auto rhi           = get_rhi();
    this->command_type = type;
    ThrowIfFailed(rhi->device->CreateCommandAllocator(type, IID_PPV_ARGS(&command_allocator)));
    reset();
}

void D3D12CommandPool::destroy()
{
    if (command_allocator) {
        command_allocator->Release();
        command_allocator = nullptr;
    }
}

void D3D12CommandPool::reset()
{
    command_allocator->Reset();
}

ID3D12GraphicsCommandList* D3D12CommandPool::allocate()
{
    auto rhi = get_rhi();

    ID3D12GraphicsCommandList* command_list = nullptr;
    ThrowIfFailed(rhi->device->CreateCommandList(0, command_type, command_allocator, NULL, IID_PPV_ARGS(&command_list)));
    return command_list;
}
