#include "MetalUtils.h"
using namespace lyra;

MetalBlas::MetalBlas() {}

MetalBlas::MetalBlas(const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors size_descs)
{
    auto rhi = get_rhi();

    // Check if device supports ray tracing
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return;
    }

    // Create primitive acceleration structure descriptor
    MTLPrimitiveAccelerationStructureDescriptor* as_desc =
        [MTLPrimitiveAccelerationStructureDescriptor new];

    NSMutableArray<MTLAccelerationStructureTriangleGeometryDescriptor*>* geometry_descriptors =
        [NSMutableArray new];

    for (auto& size_desc : size_descs) {
        if (size_desc.type == GPUBlasType::TRIANGLE) {
            MTLAccelerationStructureTriangleGeometryDescriptor* triangle_geom =
                [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];

            // Calculate triangle count from index count or vertex count
            uint triangle_count = 0;
            if (size_desc.triangles.index_count > 0) {
                triangle_count = size_desc.triangles.index_count / 3;
            } else {
                triangle_count = size_desc.triangles.vertex_count / 3;
            }
            triangle_geom.triangleCount = triangle_count;

            // Set vertex stride based on vertex format
            NSUInteger vertex_stride = sizeof(float) * 3;  // Default float3 stride
            switch (size_desc.triangles.vertex_format) {
                case GPUVertexFormat::FLOAT32x2:
                    vertex_stride = sizeof(float) * 2;
                    break;
                case GPUVertexFormat::FLOAT32x3:
                    vertex_stride = sizeof(float) * 3;
                    break;
                case GPUVertexFormat::FLOAT32x4:
                    vertex_stride = sizeof(float) * 4;
                    break;
                default:
                    vertex_stride = sizeof(float) * 3;
                    break;
            }
            triangle_geom.vertexStride = vertex_stride;

            // Set index type based on index format
            if (size_desc.triangles.index_count > 0) {
                if (size_desc.triangles.index_format == GPUIndexFormat::UINT16) {
                    triangle_geom.indexType = MTLIndexTypeUInt16;
                } else {
                    triangle_geom.indexType = MTLIndexTypeUInt32;
                }
            }

            // Set geometry flags based on BVH geometry flags
            if (size_desc.triangles.flags.contains(GPUBVHGeometryFlag::BVH_OPAQUE)) {
                triangle_geom.opaque = YES;
            }
            if (size_desc.triangles.flags.contains(GPUBVHGeometryFlag::NO_DUPLICATE_ANY_HIT_INVOCATION)) {
                triangle_geom.allowDuplicateIntersectionFunctionInvocation = NO;
            } else {
                triangle_geom.allowDuplicateIntersectionFunctionInvocation = YES;
            }

            [geometry_descriptors addObject:triangle_geom];
        }
        // AABB geometries would go here for procedural geometry
    }

    as_desc.geometryDescriptors = geometry_descriptors;

    // Set acceleration structure flags
    if (desc.flags.contains(GPUBVHFlag::ALLOW_UPDATE)) {
        as_desc.usage = MTLAccelerationStructureUsageRefit;
    } else if (desc.flags.contains(GPUBVHFlag::PREFER_FAST_TRACE)) {
        as_desc.usage = MTLAccelerationStructureUsagePreferFastBuild;
    }

    // Get acceleration structure sizes
    sizes = [rhi->device accelerationStructureSizesWithDescriptor:as_desc];

    // Allocate acceleration structure
    blas = [rhi->device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    if (!blas) {
        get_logger()->error("Failed to allocate BLAS acceleration structure");
        return;
    }

    // Store descriptor for building
    descriptor = as_desc;
}

void MetalBlas::destroy()
{
    blas = nil;
    descriptor = nil;
    sizes = {};
}

bool api::create_blas(GPUBlasHandle& handle, const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors sizes)
{
    auto rhi = get_rhi();

    // Check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return false;
    }

    auto obj = MetalBlas(desc, sizes);
    if (!obj.valid()) {
        return false;
    }

    auto ind = rhi->blases.add(obj);
    handle = GPUBlasHandle(ind);
    return true;
}

void api::delete_blas(GPUBlasHandle handle)
{
    get_rhi()->blases.remove(handle.value);
}

bool api::get_blas_sizes(GPUBlasHandle handle, GPUBVHSizes& sizes)
{
    auto rhi = get_rhi();
    auto& blas = fetch_resource(rhi->blases, handle);

    sizes.bvh_size = static_cast<uint>(blas.sizes.accelerationStructureSize);
    sizes.build_size = static_cast<uint>(blas.sizes.buildScratchBufferSize);
    sizes.update_size = static_cast<uint>(blas.sizes.refitScratchBufferSize);

    return true;
}
