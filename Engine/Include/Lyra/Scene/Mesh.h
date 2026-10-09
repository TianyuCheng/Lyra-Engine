#pragma once

#ifndef LYRA_ENGINE_SCENE_MESH_H
#define LYRA_ENGINE_SCENE_MESH_H

#include <Lyra/Utilities/Macros.h>
#include <Lyra/Assets/Format/MeshAsset.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

namespace lyra
{
    /**
     * @brief component representing a renderable mesh surface and its material.
     */
    struct [[lyra::component("Mesh", category = "Rendering", icon = "LYRA_ICON_MESH")]] Mesh
    {
        MeshAssetHandle     mesh;     ///< reference to MeshAsset.
        MaterialAssetHandle material; ///< reference to MaterialAsset.
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_MESH_H
