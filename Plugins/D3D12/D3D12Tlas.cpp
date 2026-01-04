#include <Lyra/Common/Function.h>

#include "D3D12Utils.h"

D3D12Tlas::D3D12Tlas()
{
    // do nothing
}

D3D12Tlas::D3D12Tlas(const GPUTlasDescriptor& desc)
{
    auto rhi = get_rhi();

    max_instance_count = desc.max_instances;
    update_mode        = desc.update_mode;

    // create instance buffer
    instances = lyra::execute([&]() {
        auto desc            = GPUBufferDescriptor{};
        desc.size            = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * max_instance_count;
        desc.usage           = GPUBufferUsage::COPY_DST; // Destination for staging buffer copy
        desc.virtual_address = true;
        return D3D12Buffer(desc);
    });

    // create staging buffer
    staging = lyra::execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.size               = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * max_instance_count;
        desc.usage              = GPUBufferUsage::MAP_WRITE | GPUBufferUsage::COPY_SRC;
        desc.mapped_at_creation = true;
        return D3D12Buffer(desc);
    });

    ID3D12Device5* device5 = nullptr;
    if (FAILED(rhi->device->QueryInterface(IID_PPV_ARGS(&device5)))) {
        get_logger()->error("D3D12 Ray Tracing is not supported (ID3D12Device5 not found).");
        return;
    }

    // setup build info
    build.Type        = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    build.Flags       = d3d12enum(desc.flags);
    build.NumDescs    = max_instance_count;
    build.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

    // get size requirements
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&build, &this->sizes);

    // create storage buffer
    auto storage_buffer_desc            = GPUBufferDescriptor{};
    storage_buffer_desc.size            = this->sizes.ResultDataMaxSizeInBytes;
    storage_buffer_desc.usage           = GPUBufferUsage::STORAGE;
    storage_buffer_desc.virtual_address = true;
    storage                             = D3D12Buffer(storage_buffer_desc);
    tlas                                = storage.buffer;

    if (desc.label)
        tlas->SetName(to_wstring(desc.label).c_str());

    device5->Release();
}

void D3D12Tlas::destroy()
{
    storage.destroy();
    staging.destroy();
    instances.destroy();
    tlas = nullptr;
}
