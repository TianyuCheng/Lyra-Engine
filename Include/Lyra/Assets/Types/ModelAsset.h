#ifndef LYRA_LYRA_ASSETS_TYPES_MODELASSET_H
#define LYRA_LYRA_ASSETS_TYPES_MODELASSET_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Assets/Types/MeshAsset.h>
#include <Lyra/Assets/Types/MaterialAsset.h>

namespace lyra
{

    /**
     * @brief A model asset acting as a container for hierarchy and mesh/material bindings.
     */
    struct ModelAsset
    {
        static constexpr CString name = "ModelAsset";

        static constexpr AssetTypeID type = 0xd32a1ea4;

        static auto loader() -> AssetLoaderAPI;

        // stl cooker
        struct stl
        {
            static auto cooker() -> AssetCookerAPI;
        };

        // obj cooker
        struct obj
        {
            static auto cooker() -> AssetCookerAPI;
        };

        // gltf cooker
        struct gltf
        {
            static auto cooker() -> AssetCookerAPI;
        };

        struct Node
        {
            String              name;
            Matrix4x4           transform;
            MeshAssetHandle     mesh;
            MaterialAssetHandle material;
            Vector<uint>        children;
        };

        uint         root = 0;
        Vector<Node> nodes;
    };

    using ModelAssetHandle = AssetHandle<ModelAsset>;

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_TYPES_MODELASSET_H
