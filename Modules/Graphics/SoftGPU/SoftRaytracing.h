#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTRAYTRACING_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTRAYTRACING_H

#include "SoftCommon.h"
#include "SoftBuffer.h"
#include <embree4/rtcore.h>

namespace lyra::softgpu
{
    auto get_or_create_embree_device() -> RTCDevice;

    struct SoftBlas
    {
        RTCScene         scene = nullptr;
        CString          label = "";
        Vector<uint32_t> fallback_indices;
        bool             built = false;

        SoftBlas() = default;
        explicit SoftBlas(const GPUBlasDescriptor& desc);
        ~SoftBlas();

        SoftBlas(SoftBlas&& other) noexcept;
        SoftBlas& operator=(SoftBlas&& other) noexcept;

        SoftBlas(const SoftBlas&)            = delete;
        SoftBlas& operator=(const SoftBlas&) = delete;

        auto build(const GPUBlasBuildEntry& entry, const Slotmap<SoftBuffer>& buffer_pool) -> void;
        auto intersect(const Vector3& origin, const Vector3& dir, float t_min, float t_max,
            float& hit_t, float& hit_u, float& hit_v, uint32_t& hit_geom, uint32_t& hit_prim) const -> bool;
    };

    struct SoftTlasInstance
    {
        Matrix4x4     transform{1.0f};
        GPUBlasHandle blas;
        uint32_t      custom_index = 0;
        uint8_t       mask         = 0xFF;
    };

    struct SoftTlas
    {
        RTCScene                 scene = nullptr;
        Vector<SoftTlasInstance> instances;
        uint32_t                 max_instances = 0;
        CString                  label         = "";
        bool                     built         = false;

        SoftTlas() = default;
        explicit SoftTlas(const GPUTlasDescriptor& desc);
        ~SoftTlas();

        SoftTlas(SoftTlas&& other) noexcept;
        SoftTlas& operator=(SoftTlas&& other) noexcept;

        SoftTlas(const SoftTlas&)            = delete;
        SoftTlas& operator=(const SoftTlas&) = delete;

        auto build(const GPUTlasBuildEntry& entry, const Slotmap<SoftBlas>& blas_pool) -> void;
        auto intersect(const Vector3& origin, const Vector3& dir, float t_min, float t_max, uint8_t ray_mask, const Slotmap<SoftBlas>& blas_pool,
            float& hit_t, float& hit_u, float& hit_v, uint32_t& hit_instance, uint32_t& hit_geom, uint32_t& hit_prim) const -> bool;
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
        uint8_t mask  = 0xFF;

        SoftRayQueryStatus status = SoftRayQueryStatus::NOTHING;
        float              hit_t  = 0.0f;
        Vector2            barycentrics{0.0f};
        uint32_t           instance_id  = 0;
        uint32_t           geometry_id  = 0;
        uint32_t           primitive_id = 0;

        auto trace_ray_inline(const SoftTlas& tlas, uint32_t flags, uint8_t ray_mask,
            const Vector3& orig, const Vector3& dir, float min_t, float max_t,
            const Slotmap<SoftBlas>& blas_pool) -> void;
        auto proceed() -> bool { return false; }
        auto committed_status() const -> SoftRayQueryStatus { return status; }
        auto committed_triangle_barycentrics() const -> Vector2 { return barycentrics; }
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTRAYTRACING_H
