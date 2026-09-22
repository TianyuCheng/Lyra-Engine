#pragma once

#ifndef LYRA_LYRA_SCENES_CAMERA_H
#define LYRA_LYRA_SCENES_CAMERA_H

#include <Lyra/Common/Math.h>

namespace lyra
{
    /**
     * @brief perspective camera projection parameters.
     */
    struct PerspectiveCamera
    {
        float fov        = 60.0f;
        float aspect     = 1.777f;
        float near_plane = 0.1f;
        float far_plane  = 1000.0f;
    };

    /**
     * @brief orthographic camera projection parameters.
     */
    struct OrthographicCamera
    {
        float size       = 10.0f;
        float aspect     = 1.777f;
        float near_plane = -1.0f;
        float far_plane  = 1.0f;
    };

    /**
     * @brief component to store the calculated projection matrix.
     */
    struct CameraProjection
    {
        Matrix4x4 projection = Matrix4x4(1.0f);
    };

} // namespace lyra

#endif // LYRA_LYRA_SCENES_CAMERA_H
