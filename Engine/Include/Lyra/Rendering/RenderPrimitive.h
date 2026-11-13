#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDER_PRIMITIVE_H
#define LYRA_ENGINE_RENDERING_RENDER_PRIMITIVE_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Assets/Format/MaterialAsset.h>
#include <Lyra/Rendering/Geometry.h>

namespace lyra
{
    /**
     * @brief high-level category of renderable geometric primitives.
     */
    enum struct PrimitiveType : uint
    {
        MESH,    ///< traditional indexed triangle mesh
        MESHLET, ///< clustered meshlet geometry
        SDF,     ///< analytic or voxel signed distance field
        BVH,     ///< bottom-level acceleration structure instance
    };

    /**
     * @brief geometric shapes supported by analytic signed distance fields.
     */
    enum struct SDFShapeType : uint
    {
        SPHERE,
        BOX,
        CYLINDER,
        CAPSULE,
        TORUS,
        PLANE,
    };

    /**
     * @brief constructive solid geometry (csg) boolean operations for sdfs.
     */
    enum struct SDFBlendOp : uint
    {
        UNION,
        SUBTRACTION,
        INTERSECTION,
        SMOOTH_UNION,
    };

    /**
     * @brief an extracted mesh primitive instance ready for drawing.
     */
    struct MeshPrimitive
    {
        IndexedGeometry     geometry;
        uint                submesh_index      = 0;
        uint                first_index        = 0;
        uint                index_count        = 0;
        MaterialAssetHandle material;
        Matrix4x4           transform          = Matrix4x4(1.0f);
        Bounds3D            world_bounds       = {};
        uint                instance_id        = 0;
        uint                shading_model      = 0; // 0 = PBR, 1 = Unlit, 2 = Toon, 3 = Subsurface, 4 = Custom
        uint                custom_pipeline_id = 0; // TODO: support per-primitive custom shading pipeline overrides
    };

    /**
     * @brief an extracted meshlet cluster batch ready for task/mesh dispatch.
     */
    struct MeshletPrimitive
    {
        MeshletGeometry     geometry;
        uint                meshlet_offset     = 0;
        uint                meshlet_count      = 0;
        MaterialAssetHandle material;
        Matrix4x4           transform          = Matrix4x4(1.0f);
        Bounds3D            world_bounds       = {};
        uint                instance_id        = 0;
        uint                shading_model      = 0; // 0 = PBR, 1 = Unlit, 2 = Toon, 3 = Subsurface, 4 = Custom
        uint                custom_pipeline_id = 0; // TODO: support per-primitive custom shading pipeline overrides
    };

    /**
     * @brief an extracted analytic or voxel sdf volume instance.
     */
    struct SDFPrimitive
    {
        SDFShapeType        shape_type = SDFShapeType::SPHERE;
        SDFBlendOp          blend_op   = SDFBlendOp::UNION;
        Vector4             parameters = Vector4(1.0f); // e.g. radius, half-extents, smoothness
        MaterialAssetHandle material;
        Matrix4x4           transform          = Matrix4x4(1.0f);
        Bounds3D            world_bounds       = {};
        uint                instance_id        = 0;
        uint                shading_model      = 0; // 0 = PBR, 1 = Unlit, 2 = Toon, 3 = Subsurface, 4 = Custom
        uint                custom_pipeline_id = 0; // TODO: support custom procedural SDF shaders
    };

    /**
     * @brief an extracted ray tracing acceleration structure instance.
     */
    struct BVHPrimitive
    {
        GPUBlasHandle blas;
        uint          instance_custom_index = 0;
        uint          mask                  = 0xFF;
        uint          sbt_offset            = 0;
        Matrix4x4     transform             = Matrix4x4(1.0f);
        Bounds3D      world_bounds          = {};
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDER_PRIMITIVE_H
