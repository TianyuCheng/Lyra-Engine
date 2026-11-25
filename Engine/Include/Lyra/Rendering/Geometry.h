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
     * @brief GPU geometry data representations supported by the rendering system.
     */
    enum struct GeometryForm : uint
    {
        INDEXED     = 1 << 0, ///< Traditional VBO / IBO indexed rendering.
        MESHLET     = 1 << 1, ///< Meshlet / cluster data for task & mesh shaders.
        BINDLESS    = 1 << 2, ///< Byte-addressable storage buffer representation.
        RAY_TRACING = 1 << 3, ///< Bottom-Level Acceleration Structure (BLAS).
    };
} // namespace lyra

ENABLE_BIT_FLAGS(lyra::GeometryForm)

namespace lyra
{
    using GeometryForms = BitFlags<GeometryForm>;

    /**
     * @brief Byte offsets of individual attribute streams within a consolidated vertex_slice.
     *        Unrepresented attributes are marked as OMITTED rather than allocating dummy data.
     */
    struct VertexStreamOffsets
    {
        static constexpr uint OMITTED = ~0u;

        uint position  = OMITTED; ///< Byte offset within vertex_slice.
        uint normal    = OMITTED; ///< Byte offset within vertex_slice, or OMITTED.
        uint tangent   = OMITTED; ///< Byte offset within vertex_slice, or OMITTED.
        uint bitangent = OMITTED; ///< Byte offset within vertex_slice, or OMITTED.
        uint texcoord0 = OMITTED; ///< Byte offset within vertex_slice, or OMITTED.
        uint texcoord1 = OMITTED; ///< Byte offset within vertex_slice, or OMITTED.
        uint color0    = OMITTED; ///< Byte offset within vertex_slice, or OMITTED.
        uint joints0   = OMITTED; ///< Byte offset within vertex_slice, or OMITTED (bone indices 0..3).
        uint joints1   = OMITTED; ///< Byte offset within vertex_slice, or OMITTED (bone indices 4..7).
        uint weights0  = OMITTED; ///< Byte offset within vertex_slice, or OMITTED (bone weights 0..3).
        uint weights1  = OMITTED; ///< Byte offset within vertex_slice, or OMITTED (bone weights 4..7).

        [[nodiscard]] constexpr bool has_position() const noexcept { return position != OMITTED; }
        [[nodiscard]] constexpr bool has_normal() const noexcept { return normal != OMITTED; }
        [[nodiscard]] constexpr bool has_tangent() const noexcept { return tangent != OMITTED; }
        [[nodiscard]] constexpr bool has_bitangent() const noexcept { return bitangent != OMITTED; }
        [[nodiscard]] constexpr bool has_texcoord0() const noexcept { return texcoord0 != OMITTED; }
        [[nodiscard]] constexpr bool has_texcoord1() const noexcept { return texcoord1 != OMITTED; }
        [[nodiscard]] constexpr bool has_color0() const noexcept { return color0 != OMITTED; }
        [[nodiscard]] constexpr bool has_joints0() const noexcept { return joints0 != OMITTED; }
        [[nodiscard]] constexpr bool has_weights0() const noexcept { return weights0 != OMITTED; }
        [[nodiscard]] constexpr bool has_joints1() const noexcept { return joints1 != OMITTED; }
        [[nodiscard]] constexpr bool has_weights1() const noexcept { return weights1 != OMITTED; }
        [[nodiscard]] constexpr bool has_skinning() const noexcept { return has_joints0() && has_weights0(); }
        [[nodiscard]] constexpr bool has_extended_skinning() const noexcept { return has_skinning() && has_joints1() && has_weights1(); }

        [[nodiscard]] constexpr uint bone_influences() const noexcept
        {
            if (has_extended_skinning()) return 8;
            if (has_skinning()) return 4;
            return 0;
        }
    };

    /**
     * @brief Traditional indexed geometry presentation.
     */
    struct IndexedGeometry
    {
        BufferSlice         vertex_slice;
        BufferSlice         position_slice;
        BufferSlice         index_slice;
        GPUIndexFormat      index_format = GPUIndexFormat::UINT32;
        uint                vertex_count = 0;
        uint                index_count  = 0;
        VertexStreamOffsets stream_offsets;
        GPUBindGroup        vertex_bind_group{};

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
        Optional<IndexedGeometry>    indexed;
        Optional<MeshletGeometry>    meshlet;
        Optional<RayTracingGeometry> ray_tracing;
        Vector<MeshSurface>          surfaces;
    };

    /**
     * @brief Cached GPU entry for a MeshAsset across all LODs and presentations.
     */
    struct MeshGeometryEntry
    {
        Vector<GeometryLOD> lods;
        Bounds3D            local_bounds;
        uint                ref_count       = 0;
        uint                last_used_frame = 0;
        GeometryForms       loaded_forms    = GeometryForm::INDEXED;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_GEOMETRY_H
