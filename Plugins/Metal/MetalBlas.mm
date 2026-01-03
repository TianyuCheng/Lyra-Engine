#include "MetalUtils.h"
using namespace lyra;

MetalBlas::MetalBlas() {}

MetalBlas::MetalBlas(const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors size_descs)
{
    @autoreleasepool {
        auto rhi = get_rhi();

        // check if device supports ray tracing
        if (![rhi->device supportsRaytracing]) {
            get_logger()->error("Metal ray tracing is not supported on this device");
            throw GPUValidationError("Metal ray tracing is not supported on this device");
        }

        // create primitive acceleration structure descriptor
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

                // set vertex stride based on vertex format
                NSUInteger vertex_stride = sizeof(float) * 3; // Default float3 stride
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

                // set index type based on index format
                if (size_desc.triangles.index_count > 0) {
                    if (size_desc.triangles.index_format == GPUIndexFormat::UINT16) {
                        triangle_geom.indexType = MTLIndexTypeUInt16;
                    } else {
                        triangle_geom.indexType = MTLIndexTypeUInt32;
                    }
                }

                // set geometry flags based on BVH geometry flags
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

        // set acceleration structure flags
        if (desc.flags.contains(GPUBVHFlag::ALLOW_UPDATE)) {
            as_desc.usage = MTLAccelerationStructureUsageRefit;
        } else if (desc.flags.contains(GPUBVHFlag::PREFER_FAST_TRACE)) {
            as_desc.usage = MTLAccelerationStructureUsagePreferFastBuild;
        }

        // get acceleration structure sizes
        sizes = [rhi->device accelerationStructureSizesWithDescriptor:as_desc];

        // allocate acceleration structure
        blas = [rhi->device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
        if (!blas) {
            get_logger()->error("Failed to allocate BLAS acceleration structure of size {}", (uint64_t)sizes.accelerationStructureSize);
            throw GPUOutOfMemoryError("Failed to allocate BLAS acceleration structure");
        }

        // Store descriptor for building
        descriptor = as_desc;
    }
}

void MetalBlas::destroy()
{
    @autoreleasepool {
        blas       = nil;
        descriptor = nil;
        sizes      = {};
    }
}
