#ifndef LYRA_LIBRARY_ASSETS_MESH_ASSET_H
#define LYRA_LIBRARY_ASSETS_MESH_ASSET_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Render/RHIEnums.h>

namespace lyra
{
    /**
     * @brief A mesh asset containing optimized vertex and index data.
     */
    struct MeshAsset
    {
        static constexpr CString name = "MeshAsset";

        static constexpr AssetTypeID type = 0xc599b40c;

        static auto loader() -> AssetLoaderAPI;

        struct Submesh
        {
            uint first_index;
            uint index_count;
            uint first_vertex;
            uint vertex_count;
        };

        Vector<uint8_t> vertex_data;  ///< Raw vertex data.
        Vector<uint8_t> index_data;   ///< Raw index data (usually uint32).
        Vector<Submesh> submeshes;    ///< List of submesh ranges.
        GPUIndexFormat  index_format; ///< Format of indices.
        Vector3         min_bounds;   ///< AABB min.
        Vector3         max_bounds;   ///< AABB max.
    };
} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_MESH_ASSET_H
