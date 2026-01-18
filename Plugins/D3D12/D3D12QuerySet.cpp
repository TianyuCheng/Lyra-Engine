#include "D3D12Utils.h"

D3D12QuerySet::D3D12QuerySet()
{
    pool = nullptr;
}

D3D12QuerySet::D3D12QuerySet(const GPUQuerySetDescriptor& desc)
{
    auto rhi = get_rhi();

    D3D12_QUERY_HEAP_DESC heap_desc = {};
    heap_desc.Count                 = desc.count;
    heap_desc.NodeMask              = 0;

    switch (desc.type) {
        case GPUQueryType::OCCLUSION:
            heap_desc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
            break;
        case GPUQueryType::TIMESTAMP:
            heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
            break;
        case GPUQueryType::BLAS_PROPERTIES: {
            auto buffer_desc            = GPUBufferDescriptor{};
            buffer_desc.size            = desc.count * sizeof(uint64_t);
            buffer_desc.usage           = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_SRC;
            buffer_desc.virtual_address = true;
            buffer                      = D3D12Buffer(buffer_desc);
            return;
        }
        default:
            assert(!"unsupported query type!");
            return;
    }

    this->type = desc.type;

    ThrowIfFailed(rhi->device->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(&pool)));
}

void D3D12QuerySet::destroy()
{
    if (pool != nullptr) {
        pool->Release();
        pool = nullptr;
    }

    if (buffer.valid()) {
        buffer.destroy();
    }
}
