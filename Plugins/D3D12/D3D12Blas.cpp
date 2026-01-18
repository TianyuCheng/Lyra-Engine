#include "D3D12Utils.h"

// i have to add this because the destructor is not defined in the header
#include <Lyra/Common/Function.h>

D3D12Blas::D3D12Blas()
{
    // do nothing
}

D3D12Blas::D3D12Blas(const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors sizes)
{
    auto rhi = get_rhi();

    ID3D12Device5* device5 = nullptr;
    if (FAILED(rhi->device->QueryInterface(IID_PPV_ARGS(&device5)))) {
        get_logger()->error("D3D12 Ray Tracing is not supported (ID3D12Device5 not found).");
        return;
    }

    geometries.clear();
    for (auto& size : sizes) {
        geometries.push_back({});

        // define geometry data
        auto& geometry = geometries.back();
        geometry.Type  = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry.Flags = d3d12enum(size.triangles.flags);

        // triangle geometry data
        geometry.Triangles.VertexBuffer.StartAddress  = 0;
        geometry.Triangles.VertexBuffer.StrideInBytes = size_of(d3d12enum(size.triangles.vertex_format));
        geometry.Triangles.VertexFormat               = d3d12enum(size.triangles.vertex_format);
        geometry.Triangles.VertexCount                = size.triangles.vertex_count;
        geometry.Triangles.IndexBuffer                = 0;
        geometry.Triangles.IndexCount                 = size.triangles.index_count;
        geometry.Triangles.IndexFormat                = (size.triangles.index_count > 0) ? d3d12enum(size.triangles.index_format) : DXGI_FORMAT_UNKNOWN;
    }

    // configure build info
    build.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    build.Flags          = d3d12enum(desc.flags);
    build.NumDescs       = static_cast<UINT>(geometries.size());
    build.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;
    build.pGeometryDescs = geometries.data();

    // get size requirements
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&build, &this->sizes);

    // create buffer to store blas
    auto additional             = D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;
    auto buffer_desc            = GPUBufferDescriptor{};
    buffer_desc.usage           = GPUBufferUsage::STORAGE;
    buffer_desc.size            = this->sizes.ResultDataMaxSizeInBytes;
    buffer_desc.virtual_address = true;
    storage                     = D3D12Buffer(buffer_desc, additional);
    blas                        = storage.buffer;

    // get resource device address
    if (blas)
        reference = blas->GetGPUVirtualAddress();

    if (desc.label)
        blas->SetName(to_wstring(desc.label).c_str());

    device5->Release();
}

void D3D12Blas::destroy()
{
    storage.destroy();
    blas = nullptr;
}
