#include "D3D12Utils.h"

#pragma region D3D12ObjectPool
void D3D12ObjectPool::init(uint capacity, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    // describe and create a render target view (RTV) descriptor heap.
    auto rhi = get_rhi();

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.NumDescriptors             = capacity;
    heap_desc.Type                       = type;
    heap_desc.Flags                      = flags;
    ThrowIfFailed(rhi->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)));

    // capacity
    this->capacity = capacity;

    // descriptor size
    increment = rhi->device->GetDescriptorHandleIncrementSize(type);

    reset();
}

void D3D12ObjectPool::reset()
{
    // reset count and free'd
    count = 0;
    freed.clear();
}

void D3D12ObjectPool::destroy()
{
    reset();

    heap->Release();
    heap = nullptr;
}

void D3D12ObjectPool::recycle(uint index)
{
    freed.push_back(index);
}

D3D12CPUDescriptor D3D12ObjectPool::allocate()
{
    uint index;
    if (freed.empty()) {
        index = count++; // update count
    } else {
        index = freed.back();
        freed.pop_back();
    }

    D3D12CPUDescriptor descriptor{};
    descriptor.index  = index; // record the descriptor index
    descriptor.handle = heap->GetCPUDescriptorHandleForHeapStart();
    descriptor.handle.ptr += increment * index;
    return descriptor;
}
#pragma endregion D3D12ObjectPool

#pragma region D3D12ObjectHeap
void D3D12ObjectHeap::init(uint capacity, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    this->capacity = capacity;
    heap_type      = type;
    heap_flags     = flags;

    // reset current heap index
    heap_index = 0;
}

void D3D12ObjectHeap::reset()
{
    for (auto& heap : heaps)
        heap.reset();

    // reset current heap index
    heap_index = 0;
}

void D3D12ObjectHeap::destroy()
{
    for (auto& heap : heaps)
        heap.destroy();

    heaps.clear();

    // reset current heap index
    heap_index = 0;
}

D3D12CPUDescriptor D3D12ObjectHeap::allocate()
{
    heap_index = find_pool_index();

    // record the pool/heap index
    auto descriptor = heaps.at(heap_index).allocate();
    descriptor.pool = heap_index;
    return descriptor;
}

void D3D12ObjectHeap::recycle(const D3D12CPUDescriptor& descriptor)
{
    auto& heap = heaps.at(descriptor.pool);
    heap.recycle(descriptor.index);
}

uint D3D12ObjectHeap::find_pool_index()
{
    for (size_t i = heap_index; i < heaps.size(); i++) {
        auto& heap = heaps.at(i);
        if (heap.count < capacity || !heap.freed.empty())
            return static_cast<uint>(i);
    }

    heaps.emplace_back();
    heaps.back().init(capacity, heap_type, heap_flags);
    return static_cast<uint>(heaps.size()) - 1u;
}
#pragma endregion D3D12ObjectHeap

#pragma region D3DBindGroupHeapAllocator
void D3D12BindGroupHeapAllocator::init(uint size, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    allocator.init(size);

    // describe and create a render target view (RTV) descriptor heap.
    auto rhi = get_rhi();

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.NumDescriptors             = size;
    heap_desc.Type                       = type;
    heap_desc.Flags                      = flags | D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(rhi->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)));

    // capacity
    capacity = size;

    // descriptor size
    increment = rhi->device->GetDescriptorHandleIncrementSize(type);

    reset();
}

void D3D12BindGroupHeapAllocator::reset()
{
    allocator.reset();
}

void D3D12BindGroupHeapAllocator::destroy()
{
    reset();

    heap->Release();
    heap = nullptr;
}

void D3D12BindGroupHeapAllocator::resize()
{
    assert(!!!"current not implemented!");
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12BindGroupHeapAllocator::cpu(uint index) const
{
    auto descriptor = heap->GetCPUDescriptorHandleForHeapStart();
    descriptor.ptr += index * increment;
    return descriptor;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12BindGroupHeapAllocator::gpu(uint index) const
{
    auto descriptor = heap->GetGPUDescriptorHandleForHeapStart();
    descriptor.ptr += index * increment;
    return descriptor;
}
#pragma endregion D3DBindGroupHeapAllocator

#pragma region D3D12DescriptorHeap
void D3D12DescriptorHeap::init(D3D12BindGroupHeapAllocator* allocator, uint page_size)
{
    this->allocator = allocator;
    this->page_size = page_size;
}

void D3D12DescriptorHeap::reset()
{
    for (auto& pool : pools)
        pool.reset();
}

void D3D12DescriptorHeap::destroy()
{
    for (auto& pool : pools) {
        pool.reset();
        pool.destroy();
    }
    pools.clear();
}

uint D3D12DescriptorHeap::allocate(uint allocate_count)
{
    auto& pool = find_available_pool(allocate_count);
    return pool.allocate(allocate_count);
}

D3D12DescriptorPool& D3D12DescriptorHeap::create_new_pool(uint allocate_count)
{
    pools.push_back(D3D12DescriptorPool{});

    auto& pool = pools.back();
    pool.init(allocator, std::max(page_size, allocate_count));
    return pool;
}

D3D12DescriptorPool& D3D12DescriptorHeap::find_available_pool(uint allocate_count)
{
    if (pools.empty())
        create_new_pool(allocate_count);

    auto& pool = pools.back();
    if (pool.count + allocate_count >= pool.capacity)
        create_new_pool(allocate_count);

    return pools.back();
}
#pragma endregion D3D12DescriptorHeap

#pragma region D3D12DescriptorPool
void D3D12DescriptorPool::init(D3D12BindGroupHeapAllocator* allocator, uint capacity)
{
    this->allocation = allocator->allocator.allocate(capacity);
    this->capacity   = capacity;

    assert(allocation.valid() && "Failed to allocate bind group!");
}

uint D3D12DescriptorPool::allocate(uint allocate_count)
{
    assert(count + allocate_count < capacity);

    uint current = count;
    count += allocate_count;
    return static_cast<uint>(allocation.get_offset()) + current;
}

void D3D12DescriptorPool::reset()
{
    count = 0;
}

void D3D12DescriptorPool::destroy()
{
    count = 0;
}
#pragma endregion D3D12DescriptorPool

#pragma region D3D12BindGroupHeap
D3D12BindGroupHeap::D3D12BindGroupHeap()
{
    // do nothing
}

D3D12BindGroupHeap::D3D12BindGroupHeap(const GPUBindGroupHeapDescriptor& desc)
{
    auto rhi = get_rhi();

    default_heap.init(&rhi->gpu_default_heap, desc.page_size);
    sampler_heap.init(&rhi->gpu_sampler_heap, desc.page_size);

    memory = std::make_shared<LinearMemoryAllocator<D3D12BindGroup>>(4096);
}

D3D12BindGroupHeap::~D3D12BindGroupHeap()
{
    default_heap.destroy();
    sampler_heap.destroy();
}
#pragma endregion D3D12BindGroupHeap
