// reference: https://alain.xyz/blog/raw-directx12
#include <thread>
#include "D3D12Utils.h"

#pragma region D3D12CommandPool
void D3D12CommandPool::init(D3D12_COMMAND_LIST_TYPE type)
{
    auto rhi           = get_rhi();
    this->command_type = type;
    ThrowIfFailed(rhi->device->CreateCommandAllocator(type, IID_PPV_ARGS(&command_allocator)));
    ThrowIfFailed(rhi->device->CreateCommandList(0, command_type, command_allocator, nullptr, IID_PPV_ARGS(&command_list)));
    ThrowIfFailed(command_list->Close());
    fence.init(false);
}

void D3D12CommandPool::reset()
{
    if (command_allocator) {
        ThrowIfFailed(command_allocator->Reset());
    }
    fence.reset();
}

void D3D12CommandPool::destroy()
{
    if (command_list) {
        command_list->Release();
        command_list = nullptr;
    }
    if (command_allocator) {
        command_allocator->Release();
        command_allocator = nullptr;
    }
    fence.destroy();
}
#pragma endregion D3D12CommandPool

#pragma region D3D12CommandPoolManager
void D3D12CommandPoolManager::init(D3D12_COMMAND_LIST_TYPE type)
{
    command_type = type;
}

void D3D12CommandPoolManager::destroy()
{
    LockGuard lock(mutex);
    for (auto& pool : all_pools) {
        pool->destroy();
    }
    all_pools.clear();
    unused_pools.clear();
    inflight_pools.clear();
}

auto D3D12CommandPoolManager::acquire_pool() -> D3D12CommandPool*
{
    LockGuard lock(mutex);
    process_completed_pools();

    if (!unused_pools.empty()) {
        auto* pool = unused_pools.back();
        unused_pools.pop_back();
        ThrowIfFailed(pool->command_list->Reset(pool->command_allocator, nullptr));
        return pool;
    }

    auto new_pool = make_own<D3D12CommandPool>();
    new_pool->init(command_type);
    ThrowIfFailed(new_pool->command_list->Reset(new_pool->command_allocator, nullptr));
    auto* pool_ptr = new_pool.get();
    all_pools.push_back(std::move(new_pool));
    return pool_ptr;
}

void D3D12CommandPoolManager::release_pool(
    D3D12CommandPool*       pool,
    GPUCommandEncoderHandle cmd_handle,
    ID3D12Fence*            fence_to_wait,
    ulong                   fence_value)
{
    LockGuard lock(mutex);
    inflight_pools.push_back(InflightEntry{pool, cmd_handle, fence_to_wait, fence_value});
}

void D3D12CommandPoolManager::process_completed_pools()
{
    auto rhi = get_rhi();
    for (usize i = 0; i < inflight_pools.size();) {
        auto& entry = inflight_pools[i];

        bool completed = false;
        if (!entry.fence_to_wait) {
            completed = true;
        } else if (entry.fence_to_wait->GetCompletedValue() >= entry.fence_value) {
            completed = true;
        }

        if (completed) {
            if (entry.pool) {
                entry.pool->reset();
                unused_pools.push_back(entry.pool);
            }

            if (entry.cmd_handle.valid()) {
                LockGuard cmd_lock(rhi->command_buffers_mutex);
                rhi->command_buffers.remove(entry.cmd_handle.to_slotmap_handle<D3D12CommandBuffer>());
            }

            inflight_pools[i] = inflight_pools.back();
            inflight_pools.pop_back();
        } else {
            ++i;
        }
    }
}

void D3D12CommandPoolManager::wait_idle()
{
    LockGuard lock(mutex);
    for (auto& entry : inflight_pools) {
        if (entry.fence_to_wait) {
            while (entry.fence_to_wait->GetCompletedValue() < entry.fence_value) {
                std::this_thread::yield();
            }
        }
        if (entry.pool) {
            entry.pool->reset();
            unused_pools.push_back(entry.pool);
        }
        if (entry.cmd_handle.valid()) {
            auto      rhi = get_rhi();
            LockGuard cmd_lock(rhi->command_buffers_mutex);
            rhi->command_buffers.remove(entry.cmd_handle.to_slotmap_handle<D3D12CommandBuffer>());
        }
    }
    inflight_pools.clear();
}
#pragma endregion D3D12CommandPoolManager
