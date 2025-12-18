#include "MetalUtils.h"

using namespace lyra;

MetalTlas::MetalTlas()
{
    // do nothing
}

MetalTlas::MetalTlas(const GPUTlasDescriptor& desc)
{
    auto rhi = get_rhi();

    // check if device supports ray tracing
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return;
    }

    // create instance acceleration structure descriptor
    MTLInstanceAccelerationStructureDescriptor* as_desc =
        [MTLInstanceAccelerationStructureDescriptor new];

    as_desc.instanceCount = desc.max_instances;

    // set acceleration structure usage flags
    if (desc.flags.contains(GPUBVHFlag::ALLOW_UPDATE)) {
        as_desc.usage = MTLAccelerationStructureUsageRefit;
    } else if (desc.flags.contains(GPUBVHFlag::PREFER_FAST_TRACE)) {
        as_desc.usage = MTLAccelerationStructureUsagePreferFastBuild;
    }

    // get acceleration structure sizes
    sizes = [rhi->device accelerationStructureSizesWithDescriptor:as_desc];

    // allocate acceleration structure
    tlas = [rhi->device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    if (!tlas) {
        get_logger()->error("Failed to allocate TLAS acceleration structure");
        return;
    }

    // store descriptor for building
    descriptor = as_desc;
}

void MetalTlas::destroy()
{
    tlas = nil;
    descriptor = nil;
    sizes = {};
}

bool api::create_tlas(GPUTlasHandle& handle, const GPUTlasDescriptor& desc)
{
    auto rhi = get_rhi();

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return false;
    }

    auto obj = MetalTlas(desc);
    if (!obj.valid()) {
        return false;
    }

    auto ind = rhi->tlases.add(obj);
    handle = GPUTlasHandle(ind);
    return true;
}

void api::delete_tlas(GPUTlasHandle handle)
{
    get_rhi()->tlases.remove(handle.value);
}

bool api::get_tlas_sizes(GPUTlasHandle handle, GPUBVHSizes& sizes)
{
    auto rhi = get_rhi();
    auto& tlas = fetch_resource(rhi->tlases, handle);

    sizes.bvh_size = static_cast<uint>(tlas.sizes.accelerationStructureSize);
    sizes.build_size = static_cast<uint>(tlas.sizes.buildScratchBufferSize);
    sizes.update_size = static_cast<uint>(tlas.sizes.refitScratchBufferSize);

    return true;
}
