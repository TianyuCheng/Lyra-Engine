#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDER_SCENE_H
#define LYRA_ENGINE_RENDERING_RENDER_SCENE_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/RenderPrimitive.h>

namespace lyra
{
    /**
     * @brief light type for extracted scene lights.
     */
    enum struct RenderLightType : uint
    {
        DIRECTIONAL = 0,
        POINT       = 1,
        SPOT        = 2,
    };

    /**
     * @brief flat description of an extracted analytical light.
     */
    struct RenderLight
    {
        Vector3         position         = Vector3(0.0f);
        Vector3         direction        = Vector3(0.0f, -1.0f, 0.0f);
        Vector3         color            = Vector3(1.0f);
        float           intensity        = 100.0f;
        float           range            = 20.0f;
        float           spot_inner_angle = 0.0f;
        float           spot_outer_angle = 0.785398f; // pi / 4
        RenderLightType type             = RenderLightType::POINT;
        bool            cast_shadows     = true;
    };

    /**
     * @brief flat, cache-friendly scene packet extracted per-frame for renderer consumption.
     */
    struct RenderScene
    {
        // instanced primitive collections
        Vector<MeshPrimitive>    meshes;
        Vector<MeshletPrimitive> meshlets;
        Vector<SDFPrimitive>     sdfs;
        Vector<BVHPrimitive>     bvh_instances;

        // hardware acceleration structure (built when ray tracing is active)
        GPUTlasHandle scene_tlas;

        // lighting and environment
        Vector<RenderLight>  directional_lights;
        Vector<RenderLight>  local_lights;
        GPUTextureViewHandle environment_map;
        Vector3              ambient_color     = Vector3(0.03f);
        float                ambient_intensity = 1.0f;
        Vector4              clear_color       = Vector4(0.18f, 0.18f, 0.20f, 1.0f); // neutral engine gray (similar to Unreal/Blender)

        /**
         * @brief clear all extracted primitive and light buffers.
         */
        void clear()
        {
            meshes.clear();
            meshlets.clear();
            sdfs.clear();
            bvh_instances.clear();
            directional_lights.clear();
            local_lights.clear();
            scene_tlas      = {};
            environment_map = {};
            clear_color     = Vector4(0.18f, 0.18f, 0.20f, 1.0f);
        }

        [[nodiscard]] bool empty() const
        {
            return meshes.empty() && meshlets.empty() && sdfs.empty() && bvh_instances.empty();
        }
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDER_SCENE_H
