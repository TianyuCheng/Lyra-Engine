#pragma once

#ifndef LYRA_ENGINE_RENDERING_SHADING_BUFFERS_H
#define LYRA_ENGINE_RENDERING_SHADING_BUFFERS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

namespace lyra
{
    /**
     * @brief Uniform buffer layout for camera view & projection parameters matching Common/Camera.slang.
     */
    struct alignas(16) CameraUniforms
    {
        Matrix4x4 view           = Matrix4x4(1.0f);
        Matrix4x4 proj           = Matrix4x4(1.0f);
        Matrix4x4 view_proj      = Matrix4x4(1.0f);
        Matrix4x4 inv_view       = Matrix4x4(1.0f);
        Matrix4x4 inv_proj       = Matrix4x4(1.0f);
        Matrix4x4 inv_view_proj  = Matrix4x4(1.0f);
        Matrix4x4 prev_view_proj = Matrix4x4(1.0f);

        Vector3 camera_pos = Vector3(0.0f);
        float   near_plane = 0.1f;

        Vector2 jitter_offset = Vector2(0.0f);
        Vector2 render_extent = Vector2(0.0f);

        float   far_plane = 1000.0f;
        float   fov       = 60.0f;
        Vector2 padding   = Vector2(0.0f);

        static auto from_view(const RenderView& view) -> CameraUniforms
        {
            CameraUniforms u{};
            u.view           = view.view;
            u.proj           = view.projection;
            u.view_proj      = view.view_projection;
            u.inv_view       = view.inv_view;
            u.inv_proj       = view.inv_projection;
            u.inv_view_proj  = view.inv_view_projection;
            u.prev_view_proj = view.prev_view_projection;
            u.camera_pos     = view.camera_position;
            u.near_plane     = view.near_plane;
            u.jitter_offset  = view.jitter_offset;
            u.render_extent  = Vector2(static_cast<float>(view.render_extent.width), static_cast<float>(view.render_extent.height));
            u.far_plane      = view.far_plane;
            u.fov            = view.fov;
            return u;
        }
    };
    static_assert(sizeof(CameraUniforms) == 496, "CameraUniforms must be 496 bytes matching Camera.slang");

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

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_SHADING_BUFFERS_H
