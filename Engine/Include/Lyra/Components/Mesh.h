#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_MESH_H
#define LYRA_ENGINE_COMPONENTS_MESH_H

#include <Lyra/Utilities/Macros.h>
#include <Lyra/Assets/Format/MeshAsset.h>
#include <Lyra/Assets/Format/MaterialAsset.h>
#include <Lyra/Rendering/Geometry.h>

namespace lyra
{
    /**
     * @brief Component representing a renderable mesh surface and its material.
     */
    struct [[lyra::component("Mesh", category = "Rendering", icon = "LYRA_ICON_MESH")]] Mesh
    {
        MeshAssetHandle       mesh;                                                      ///< reference to MeshAsset.
        MaterialAssetHandle   material;                                                  ///< reference to MaterialAsset.
        GeometryForms       preferred_forms = GeometryForm::INDEXED; ///< requested geometry forms.
        bool                  cast_shadows            = true;                            ///< whether this mesh casts shadows.
    };

} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_MESH_H
