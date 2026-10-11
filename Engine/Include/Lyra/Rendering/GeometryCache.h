#pragma once

#ifndef LYRA_ENGINE_RENDERING_GEOMETRY_CACHE_H
#define LYRA_ENGINE_RENDERING_GEOMETRY_CACHE_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Assets/Format/MeshAsset.h>

namespace lyra
{
    struct RenderLayer;

    /**
     * @brief GPU-resident representation of a single mesh LOD level.
     */
    struct GPUMeshLOD
    {
        GPUBuffer           vertex_buffer;
        GPUBuffer           index_buffer;
        GPUIndexFormat      index_format = GPUIndexFormat::UINT32;
        uint                index_count  = 0;
        uint                vertex_count = 0;
        Vector<MeshSurface> surfaces;
    };

    /**
     * @brief Cached GPU entry for a MeshAsset.
     */
    struct GPUMeshEntry
    {
        Vector<GPUMeshLOD> lods;
        Bounds3D           local_bounds;
        uint               ref_count = 0;
    };

    /**
     * @brief GeometryCache manages GPU vertex and index buffer residency for MeshAssets.
     *
     * Provides reference-counted deduplication across entities sharing the same MeshAsset,
     * uploads interleaved vertex attributes and index data to GPU buffers, and safely defers
     * buffer destruction through RenderLayer's deletion queues.
     */
    class GeometryCache
    {
    public:
        explicit GeometryCache(RenderLayer* render_layer = nullptr);
        ~GeometryCache();

        GeometryCache(const GeometryCache&)            = delete;
        GeometryCache& operator=(const GeometryCache&) = delete;

        GeometryCache(GeometryCache&&) noexcept            = default;
        GeometryCache& operator=(GeometryCache&&) noexcept = default;

        /**
         * @brief Set or update the owning RenderLayer used for deferred deletions.
         */
        void set_render_layer(RenderLayer* layer);

        /**
         * @brief Acquire or upload GPU mesh buffers for a given MeshAsset.
         * Increments the reference count if already uploaded.
         * @return Pointer to the cached GPUMeshEntry, or nullptr if asset is empty or upload failed.
         */
        auto acquire(const MeshAssetHandle& handle, const MeshAsset& asset) -> const GPUMeshEntry*;

        /**
         * @brief Release a reference to a cached GPU mesh.
         * When ref_count drops to zero, the GPU buffers are enqueued for deferred deletion.
         */
        void release(const MeshAssetHandle& handle);

        /**
         * @brief Fast lookup of an already resident mesh without modifying reference count.
         */
        auto get(const MeshAssetHandle& handle) const -> const GPUMeshEntry*;

        /**
         * @brief Check if a mesh is currently GPU resident.
         */
        bool contains(const MeshAssetHandle& handle) const;

        /**
         * @brief Immediately drain and clear all cached GPU meshes.
         */
        void clear();

        /**
         * @brief Number of distinct cached meshes.
         */
        size_t size() const { return cache.size(); }

        /**
         * @brief Total count of active references across all cached meshes.
         */
        size_t total_references() const;

    private:
        auto upload_lod(const MeshLOD& lod, uint lod_index) -> GPUMeshLOD;

    private:
        RenderLayer*                           render_layer = nullptr;
        HashMap<MeshAssetHandle, GPUMeshEntry> cache;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_GEOMETRY_CACHE_H
