#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_SDF_VOLUME_H
#define LYRA_ENGINE_COMPONENTS_SDF_VOLUME_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Rendering/RenderPrimitive.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

namespace lyra
{
    /**
     * @brief Component representing an analytic or voxel signed distance field volume.
     */
    struct [[lyra::component("SDF Volume", category = "Rendering", icon = "LYRA_ICON_MESH")]] SDFVolume
    {
        SDFShapeType        shape_type = SDFShapeType::SPHERE;
        SDFBlendOp          blend_op   = SDFBlendOp::UNION;
        Vector4             parameters = Vector4(1.0f); // radius / half-extents / smoothness
        MaterialAssetHandle material;
    };
} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_SDF_VOLUME_H
