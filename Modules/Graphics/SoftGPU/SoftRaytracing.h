#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTRAYTRACING_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTRAYTRACING_H

#include "SoftCommon.h"
#include "SoftBuffer.h"
#include <embree4/rtcore.h>

namespace lyra::softgpu
{
    RTCDevice get_or_create_embree_device();

    struct SoftBlas
    {
        RTCScene     scene = nullptr;
        CString      label = "";
        Vector<uint> fallback_indices;
        bool         built = false;

        SoftBlas() = default;
        explicit SoftBlas(const GPUBlasDescriptor& desc);
        ~SoftBlas();

        SoftBlas(SoftBlas&& other) noexcept;
        SoftBlas& operator=(SoftBlas&& other) noexcept;

        SoftBlas(const SoftBlas&)            = delete;
        SoftBlas& operator=(const SoftBlas&) = delete;

        void build(const GPUBlasBuildEntry& entry, const SoftResourceManager<SoftBuffer>& buffer_pool);
        bool intersect(const Vector3& origin, const Vector3& dir, float t_min, float t_max,
            float& hit_t, float& hit_u, float& hit_v, uint& hit_geom, uint& hit_prim) const;
        void destroy();
    };

    struct SoftTlasInstance
    {
        Matrix4x4     transform{1.0f};
        GPUBlasHandle blas;
        uint          custom_index = 0;
        ubyte         mask         = 0xFF;
    };

    struct SoftTlas
    {
        RTCScene                 scene = nullptr;
        Vector<SoftTlasInstance> instances;
        uint                     max_instances = 0;
        CString                  label         = "";
        bool                     built         = false;

        SoftTlas() = default;
        explicit SoftTlas(const GPUTlasDescriptor& desc);
        ~SoftTlas();

        SoftTlas(SoftTlas&& other) noexcept;
        SoftTlas& operator=(SoftTlas&& other) noexcept;

        SoftTlas(const SoftTlas&)            = delete;
        SoftTlas& operator=(const SoftTlas&) = delete;

        void build(const GPUTlasBuildEntry& entry, const SoftResourceManager<SoftBlas>& blas_pool);
        bool intersect(const Vector3& origin, const Vector3& dir, float t_min, float t_max, ubyte ray_mask, const SoftResourceManager<SoftBlas>& blas_pool,
            float& hit_t, float& hit_u, float& hit_v, uint& hit_instance, uint& hit_geom, uint& hit_prim) const;
        void destroy();
    };

    enum struct SoftRayQueryStatus
    {
        NOTHING,
        TRIANGLE_HIT,
    };

    struct SoftRayQuery
    {
        Vector3 origin{0.0f};
        Vector3 direction{0.0f};
        float   t_min = 0.001f;
        float   t_max = 10000.0f;
        ubyte   mask  = 0xFF;

        SoftRayQueryStatus status = SoftRayQueryStatus::NOTHING;
        float              hit_t  = 0.0f;
        Vector2            barycentrics{0.0f};
        uint               instance_id  = 0;
        uint               geometry_id  = 0;
        uint               primitive_id = 0;

        void               trace_ray_inline(const SoftTlas& tlas, uint flags, ubyte ray_mask,
            const Vector3& orig, const Vector3& dir, float min_t, float max_t,
            const SoftResourceManager<SoftBlas>& blas_pool);
        bool               proceed() { return false; }
        SoftRayQueryStatus committed_status() const { return status; }
        Vector2            committed_triangle_barycentrics() const { return barycentrics; }
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTRAYTRACING_H
