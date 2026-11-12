#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDER_VIEW_H
#define LYRA_ENGINE_RENDERING_RENDER_VIEW_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>

namespace lyra
{
    /**
     * @brief extracted view and camera parameters for a single render target or eye.
     */
    struct RenderView
    {
        Matrix4x4 view                 = Matrix4x4(1.0f);
        Matrix4x4 projection           = Matrix4x4(1.0f);
        Matrix4x4 view_projection      = Matrix4x4(1.0f);
        Matrix4x4 inv_view             = Matrix4x4(1.0f);
        Matrix4x4 inv_projection       = Matrix4x4(1.0f);
        Matrix4x4 inv_view_projection  = Matrix4x4(1.0f);
        Matrix4x4 prev_view_projection = Matrix4x4(1.0f);

        Vector3     camera_position = Vector3(0.0f);
        Vector2     jitter_offset   = Vector2(0.0f);
        GPUExtent2D render_extent   = {1280, 720};
        float       near_plane      = 0.1f;
        float       far_plane       = 1000.0f;
        float       fov             = 60.0f;

        Array<Vector4, 6> frustum_planes = {};
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDER_VIEW_H
