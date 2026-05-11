#ifndef LYRA_LYRA_FORMAT_MESHASSET_H
#define LYRA_LYRA_FORMAT_MESHASSET_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Render/RHIEnums.h>
#include <Lyra/Format/MaterialAsset.h>

namespace lyra
{

    /**
     * @brief Semantics for mesh attributes.
     */
    enum struct MeshSemantics : uint32_t
    {
        POSITION,
        NORMAL,
        TANGENT,
        BITANGENT,
        COLOR0,
        COLOR1,
        COLOR2,
        COLOR3,
        TEXCOORD0,
        TEXCOORD1,
        TEXCOORD2,
        TEXCOORD3,
        TEXCOORD4,
        TEXCOORD5,
        TEXCOORD6,
        TEXCOORD7,
        JOINTS0,
        JOINTS1,
        WEIGHTS0,
        WEIGHTS1,
        CUSTOM0,
        CUSTOM1,
        CUSTOM2,
        CUSTOM3,
        CUSTOM4,
        CUSTOM5,
        CUSTOM6,
        CUSTOM7,
    };

    /**
     * @brief A single vertex attribute stream.
     */
    struct MeshAttribute
    {
        MeshSemantics   semantics;
        GPUVertexFormat format;
        uint32_t        element_count;
        Vector<uint8_t> data;
    };

    /**
     * @brief Range in geometry buffers.
     */
    struct GeometrySlice
    {
        uint32_t first_index;
        uint32_t index_count;
        uint32_t first_vertex;
        uint32_t vertex_count;
    };

    /**
     * @brief A logical submesh with material and bounds.
     */
    struct MeshSurface
    {
        GeometrySlice       slice;
        MaterialAssetHandle material;
        Vector3             min_bounds;
        Vector3             max_bounds;

        // Space for Meshlet info (placeholder)
        uint32_t first_meshlet = 0;
        uint32_t meshlet_count = 0;
    };

    /**
     * @brief Level of detail.
     */
    struct MeshLOD
    {
        Vector<MeshAttribute> attributes;   ///< Vertex attribute streams for this LOD.
        Vector<MeshSurface>   surfaces;     ///< Mesh surface for this LOD.
        Vector<uint8_t>       index_data;   ///< Raw index data for this LOD.
        GPUIndexFormat        index_format; ///< Format of indices for this LOD.
    };

    /**
     * @brief A mesh asset containing geometry description.
     */
    struct MeshAsset
    {
        static constexpr CString name = "MeshAsset";

        static constexpr AssetTypeID type = 0xc599b40c;

        static constexpr uint MESH_ASSET_VERSION = 0;

        static constexpr uint MESH_MAGIC = 0x534D594C; // 'LYMS'

        static constexpr uint CHUNK_BBOX = 0x584F4242; // 'BBOX'

        static constexpr uint CHUNK_ATTR = 0x52545441; // 'ATTR'

        static constexpr uint CHUNK_INDX = 0x58444E49; // 'INDX'

        static constexpr uint CHUNK_LODS = 0x53444F4C; // 'LODS'

        static auto loader() -> AssetLoaderAPI;

        bool save(OSPath path) const;

        Vector<MeshLOD> lods; ///< Levels of detail.

        Vector3 min_bounds; ///< Global AABB min.
        Vector3 max_bounds;

        // Future room for Skeleton (placeholder)
        // AssetHandle<SkeletonAsset> skeleton;
    };

    using MeshAssetHandle = AssetHandle<MeshAsset>;

} // namespace lyra

#endif // LYRA_LYRA_FORMAT_MESHASSET_H
