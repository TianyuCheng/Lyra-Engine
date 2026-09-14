#pragma once

#ifndef LYRA_LYRA_FORMAT_SCENEASSET_H
#define LYRA_LYRA_FORMAT_SCENEASSET_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Format/MeshAsset.h>
#include <Lyra/Format/ModelAsset.h>
#include <Lyra/Format/MaterialAsset.h>

namespace lyra
{

    /**
     * @brief A scene asset representing an authored scene.
     *
     * Stored in Assets/ directory as a USDA file (.scene).
     * Composes entities, lights, cameras, and can instantiate ModelAsset prefabs.
     */
    struct SceneAsset
    {
        static constexpr CString     name = "SceneAsset";
        static constexpr AssetTypeID type = 0xf1a23b4c;

        static auto loader() -> AssetLoaderAPI;
        static auto saver() -> AssetSaverAPI;

        struct Node
        {
            String              name;
            Matrix4x4           transform = Matrix4x4(1.0f);
            ModelAssetHandle    model;    ///< Optional prefab reference.
            MeshAssetHandle     mesh;     ///< Direct mesh reference.
            MaterialAssetHandle material; ///< Direct material reference.
            Vector<uint>        children; ///< Indices into the nodes array.
        };

        uint         root = 0;
        Vector<Node> nodes;

        /**
         * @brief Create a SceneAsset by converting a ModelAsset prefab.
         */
        static SceneAsset from_model(const ModelAsset& model);
    };

    using SceneAssetHandle = AssetHandle<SceneAsset>;

} // namespace lyra

#endif // LYRA_LYRA_FORMAT_SCENEASSET_H
