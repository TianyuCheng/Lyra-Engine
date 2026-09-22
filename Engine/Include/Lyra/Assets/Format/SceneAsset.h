#pragma once

#ifndef LYRA_ENGINE_ASSETS_FORMAT_SCENEASSET_H
#define LYRA_ENGINE_ASSETS_FORMAT_SCENEASSET_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Assets/Format/MeshAsset.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

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
        static constexpr AssetTypeID type = make_uuid("f1a23b4c-0008-4187-f665-8374a9c8f008");

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

#endif // LYRA_ENGINE_ASSETS_FORMAT_SCENEASSET_H
