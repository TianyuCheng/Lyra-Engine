#pragma once

#ifndef LYRA_ENGINE_RENDERING_SHADING_BUFFERS_H
#define LYRA_ENGINE_RENDERING_SHADING_BUFFERS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

namespace lyra
{
    /**
     * @brief GPU uniform buffer layout for camera view & projection parameters.
     */
    struct alignas(16) GPUCameraUniforms
    {
        Matrix4x4 view;
        Matrix4x4 proj;
        Matrix4x4 view_proj;
        Matrix4x4 inv_view;
        Matrix4x4 inv_proj;
        Matrix4x4 inv_view_proj;
        Matrix4x4 prev_view_proj;

        Vector3 camera_pos;
        float   near_plane;

        Vector2 jitter_offset;
        Vector2 render_extent;

        float   far_plane;
        float   fov;
        Vector2 padding;
    };

    /**
     * @brief GPU storage buffer layout for a single analytical light (64 bytes).
     */
    struct alignas(16) GPULightData
    {
        Vector3 position;
        float   range;

        Vector3 direction;
        float   intensity;

        Vector3 color;
        uint    type; // 0 = directional, 1 = point, 2 = spot

        float spot_inner_angle;
        float spot_outer_angle;
        uint  cast_shadows;
        float padding;
    };

    static_assert(sizeof(GPULightData) == 64, "GPULightData must be 64 bytes aligned for GPU storage buffers");

    /**
     * @brief GPU storage buffer layout for a single instance transform and bounds.
     */
    struct alignas(16) GPUInstanceData
    {
        Matrix4x4 model;
        Matrix4x4 prev_model;
        Vector3   bounds_center;
        float     bounds_radius;
        uint      material_id;
        uint      custom_index;
        Vector2   padding;
    };

    /**
     * @brief GPU material parameters for PBR and user custom material shading.
     */
    struct alignas(16) GPUMaterialData
    {
        Vector4 base_color_factor;
        Vector3 emissive_factor;
        float        roughness_factor;
        float        metallic_factor;
        ShadingModel shading_model = ShadingModel::PBR;
        uint         base_color_texture_idx;
        uint    normal_texture_idx;
        uint    roughness_metallic_texture_idx;
        uint    custom_params_offset; // offset into custom material buffer
        Vector3 custom_data;          // general-purpose extra material vector for custom shading
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_SHADING_BUFFERS_H
