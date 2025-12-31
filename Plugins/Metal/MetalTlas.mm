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
        get_logger()->error("Failed to allocate TLAS acceleration structure of size {}", (uint64_t)sizes.accelerationStructureSize);
        return;
    }

    // store descriptor for building
    descriptor = as_desc;
}

void MetalTlas::destroy()
{
    tlas       = nil;
    descriptor = nil;
    sizes      = {};
}
