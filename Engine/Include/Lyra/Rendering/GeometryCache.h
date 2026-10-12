#pragma once

#ifndef LYRA_ENGINE_RENDERING_GEOMETRY_CACHE_H
#define LYRA_ENGINE_RENDERING_GEOMETRY_CACHE_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Rendering/Geometry.h>
#include <Lyra/Rendering/GeometryBufferPool.h>

namespace lyra
{
    class ResourceRetire;

    /**
     * @brief Staging copy descriptor for geometry uploads.
     */
    struct GeometryCopy
    {
        GPUBufferHandle dst_buffer;
        GPUSize64       src_offset = 0;
        GPUSize64       dst_offset = 0;
        GPUSize64       size       = 0;
    };

    /**
     * @brief GeometryCache manages GPU memory residency, sub-allocation, multi-presentation,
     * and two-tier LRU caching for MeshAssets.
     */
    class GeometryCache
    {
    public:
        static constexpr usize DEFAULT_VRAM_BUDGET = 512 * 1024 * 1024; // 512 MB
        static constexpr uint  DEFAULT_STANDBY_TTL = 300;               // 300 frames (~5s at 60fps)

        explicit GeometryCache(ResourceRetire& resource_retire, GPUSize64 slab_size = GeometryBufferPool::DEFAULT_SLAB_SIZE);
        virtual ~GeometryCache();

        GeometryCache(const GeometryCache&)            = delete;
        GeometryCache& operator=(const GeometryCache&) = delete;

        GeometryCache(GeometryCache&&) noexcept            = default;
        GeometryCache& operator=(GeometryCache&&) noexcept = default;

        /**
         * @brief Acquire or upload GPU representations for a MeshAsset.
         * Increments the reference count if already uploaded.
         */
        auto acquire(
            const MeshAssetHandle& handle,
            const MeshAsset&       asset,
            GeometryPresentations  presentations = GeometryPresentation::TRADITIONAL)
            -> const MeshGeometryEntry*;

        /**
         * @brief On-demand acquisition of meshlet presentation for an asset.
         */
        bool acquire_meshlet(const MeshAssetHandle& handle, const MeshAsset& asset);

        /**
         * @brief On-demand acquisition/build of BLAS for ray tracing.
         */
        bool acquire_ray_tracing(const MeshAssetHandle& handle, const MeshAsset& asset);

        /**
         * @brief Release a reference. When ref_count drops to 0, moves entry to standby LRU cache.
         */
        void release(const MeshAssetHandle& handle);

        /**
         * @brief Fast lookup of an already resident mesh without modifying reference count.
         */
        auto get(const MeshAssetHandle& handle) const -> const MeshGeometryEntry*;

        /**
         * @brief Check if a mesh is currently GPU resident (active or standby).
         */
        bool contains(const MeshAssetHandle& handle) const;

        /**
         * @brief Periodic frame tick for standby LRU eviction and memory budget maintenance.
         */
        void tick(uint current_frame = 0);

        /**
         * @brief Set maximum VRAM budget for pooled geometry in bytes.
         */
        void set_vram_budget(usize bytes) { vram_budget = bytes; }

        /**
         * @brief Set time-to-live for standby meshes in frames.
         */
        void set_standby_ttl_frames(uint frames) { standby_ttl_frames = frames; }

        /**
         * @brief Immediately drain and clear all active and standby cached GPU meshes.
         */
        void clear();

        /**
         * @brief Number of active cached meshes (ref_count > 0).
         */
        usize active_count() const { return active_cache.size(); }

        /**
         * @brief Number of standby cached meshes (ref_count == 0).
         */
        usize standby_count() const { return standby_cache.size(); }

        /**
         * @brief Total count of active references across all cached meshes.
         */
        usize total_references() const;

        /**
         * @brief Total allocated VRAM capacity across buffer slabs in bytes.
         */
        GPUSize64 get_vram_allocated() const { return buffer_pool.get_total_allocated(); }

        /**
         * @brief Active sub-allocated geometry bytes.
         */
        GPUSize64 get_vram_used() const { return buffer_pool.get_used_bytes(); }

        /**
         * @brief Direct access to underlying GeometryBufferPool.
         */
        GeometryBufferPool&       get_buffer_pool() { return buffer_pool; }
        const GeometryBufferPool& get_buffer_pool() const { return buffer_pool; }

    private:
        void upload_presentations(MeshGeometryEntry& entry, const MeshAsset& asset, GeometryPresentations requested);
        void upload_traditional_lod(GeometryLOD& gpu_lod, const MeshLOD& lod, Vector<GeometryCopy>& copy_ops, Vector<uint8_t>& cpu_staging);
        void upload_meshlet_lod(GeometryLOD& gpu_lod, const MeshLOD& lod, Vector<GeometryCopy>& copy_ops, Vector<uint8_t>& cpu_staging);
        void free_entry_geometry(MeshGeometryEntry& entry);

    private:
        ResourceRetire&                             resource_retire;
        GeometryBufferPool                          buffer_pool;
        HashMap<MeshAssetHandle, MeshGeometryEntry> active_cache;
        HashMap<MeshAssetHandle, MeshGeometryEntry> standby_cache;
        usize                                       vram_budget        = DEFAULT_VRAM_BUDGET;
        uint                                        standby_ttl_frames = DEFAULT_STANDBY_TTL;
        uint                                        current_frame      = 0;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_GEOMETRY_CACHE_H
