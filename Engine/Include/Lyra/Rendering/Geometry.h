#pragma once

#ifndef LYRA_ENGINE_RENDERING_GEOMETRY_H
#define LYRA_ENGINE_RENDERING_GEOMETRY_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Assets/Format/MeshAsset.h>

namespace lyra
{
    /**
     * @brief A sub-allocated byte range within a GPU buffer slab.
     */
    struct BufferSlice
    {
        GPUBufferHandle buffer;      ///< Underlying GPU buffer handle.
        GPUSize64       offset  = 0; ///< Byte offset within the slab.
        GPUSize64       size    = 0; ///< Sub-allocated byte size.
        uint            slab_id = 0; ///< Internal pool slab identifier.

        [[nodiscard]] bool valid() const { return size > 0 && (buffer.valid() || slab_id > 0); }
    };

    /**
     * @brief Geometry presentation formats supported by the rendering system.
     */
    enum struct GeometryPresentation : uint
    {
        TRADITIONAL  = 1 << 0, ///< Traditional VBO / IBO indexed rendering.
        MESHLET      = 1 << 1, ///< Meshlet / cluster data for task & mesh shaders.
        RAY_TRACING  = 1 << 2, ///< Bottom-Level Acceleration Structure (BLAS).
        BINDLESS_RAW = 1 << 3, ///< Byte-addressable storage buffer representation.
    };
} // namespace lyra

ENABLE_BIT_FLAGS(lyra::GeometryPresentation)

namespace lyra
{
    using GeometryPresentations = BitFlags<GeometryPresentation>;

    /**
     * @brief Traditional indexed geometry presentation.
     */
    struct IndexedGeometry
    {
        BufferSlice    vertex_slice;
        BufferSlice    position_slice;
        BufferSlice    index_slice;
        GPUIndexFormat index_format = GPUIndexFormat::UINT32;
        uint           vertex_count = 0;
        uint           index_count  = 0;

        [[nodiscard]] bool valid() const { return vertex_slice.valid() && index_slice.valid(); }
    };

    /**
     * @brief GPU descriptor for a single cluster/meshlet (32 bytes).
     */
    struct MeshletDesc
    {
        Vector3 center;          ///< Bounding sphere center.
        float   radius;          ///< Bounding sphere radius.
        int8_t  cone_axis[3];    ///< Normal cone axis for backface culling.
        int8_t  cone_cutoff;     ///< cos(angle / 2).
        uint    vertex_offset;   ///< Offset in meshlet_vertex_slice.
        uint    triangle_offset; ///< Offset in meshlet_triangle_slice.
        ushort  vertex_count;    ///< Max 64 or 128 vertices.
        ushort  triangle_count;  ///< Max 126 triangles.
    };

    static_assert(sizeof(MeshletDesc) == 32, "MeshletDesc must be exactly 32 bytes for GPU cache line alignment");

    /**
     * @brief Meshlet presentation for task and mesh shader pipelines.
     */
    struct MeshletGeometry
    {
        BufferSlice meshlet_slice;          ///< Array of MeshletDesc (storage buffer).
        BufferSlice meshlet_vertex_slice;   ///< Vertex indirection table.
        BufferSlice meshlet_triangle_slice; ///< Packed micro-indices.
        BufferSlice vertex_storage_slice;   ///< Storage buffer view of vertex attributes.
        uint        meshlet_count = 0;

        [[nodiscard]] bool valid() const { return meshlet_slice.valid() && meshlet_count > 0; }
    };

    /**
     * @brief Ray tracing presentation wrapping a Bottom-Level Acceleration Structure.
     */
    struct RayTracingGeometry
    {
        GPUBlas blas;
        bool    is_built = false;

        [[nodiscard]] bool valid() const { return blas.valid(); }
    };

    /**
     * @brief GPU-resident representation of a single level of detail.
     */
    struct GeometryLOD
    {
        Optional<IndexedGeometry>    traditional;
        Optional<MeshletGeometry>    meshlet;
        Optional<RayTracingGeometry> ray_tracing;
        Vector<MeshSurface>          surfaces;
    };

    /**
     * @brief Cached GPU entry for a MeshAsset across all LODs and presentations.
     */
    struct MeshGeometryEntry
    {
        Vector<GeometryLOD>   lods;
        Bounds3D              local_bounds;
        uint                  ref_count            = 0;
        uint                  last_used_frame      = 0;
        GeometryPresentations loaded_presentations = GeometryPresentation::TRADITIONAL;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_GEOMETRY_H
