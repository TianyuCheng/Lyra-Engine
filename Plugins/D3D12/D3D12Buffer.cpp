// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

D3D12Buffer::D3D12Buffer()
{
    // do nothing
}

D3D12Buffer::D3D12Buffer(const GPUBufferDescriptor& desc, D3D12_RESOURCE_FLAGS additional_flags)
{
    // figure out correct size
    size_ = desc.size;

    // check if the buffer is going to be used for uniform buffer,
    // and adjust the buffer size allocation for the uniform buffer alignment.
    if (desc.usage.contains(GPUBufferUsage::UNIFORM)) {
        size_ = (size_ + 255) & ~255; // CBV alignment
    }

    D3D12MA::ALLOCATION_DESC allocation_desc{};
    allocation_desc.HeapType = infer_heap_type(desc.usage);

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width            = size_;
    resource_desc.Height           = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels        = 1;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Alignment        = 0;
    resource_desc.Flags            = infer_buffer_flags(desc.usage) | additional_flags;

    auto rhi = get_rhi();
    ThrowIfFailed(rhi->allocator->CreateResource(
        &allocation_desc,
        &resource_desc,
        D3D12_RESOURCE_STATE_COMMON,
        NULL,
        &allocation,
        IID_PPV_ARGS(&buffer)));

    if (desc.mapped_at_creation) map();

    if (desc.label)
        buffer->SetName(to_wstring(desc.label).c_str());
}

void D3D12Buffer::destroy()
{
    if (buffer) {
        buffer->Release();
        buffer = nullptr;
    }

    if (allocation) {
        allocation->Release();
        allocation = nullptr;
    }
}

void D3D12Buffer::map(GPUSize64 offset, GPUSize64 size)
{
    if (mapped()) unmap();

    D3D12_RANGE range = {};
    range.Begin       = offset;
    range.End         = offset + size;

    void* mapped;
    buffer->Map(0, &range, &mapped);
    mapped_data = reinterpret_cast<uint8_t*>(mapped);
    mapped_size = size == 0 ? allocation->GetSize() : size;
}

void D3D12Buffer::unmap()
{
    if (mapped_data) {
        buffer->Unmap(0, nullptr);
        mapped_data = nullptr;
        mapped_size = 0ull;
    }
}
