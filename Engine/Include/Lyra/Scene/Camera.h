#pragma once

#ifndef LYRA_ENGINE_SCENE_CAMERA_H
#define LYRA_ENGINE_SCENE_CAMERA_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    /**
     * @brief camera projection mode.
     */
    enum struct ProjectionType : uint
    {
        PERSPECTIVE,
        ORTHOGRAPHIC
    };

    /**
     * @brief unified camera component with projection parameters and cached projection matrix.
     */
    struct [[lyra::component("Camera")]] Camera
    {
        [[lyra::label("Projection")]]
        ProjectionType type = ProjectionType::PERSPECTIVE;

        [[lyra::range(1.0f, 179.0f), lyra::label("FOV"), lyra::condition("type == ProjectionType::PERSPECTIVE")]]
        float fov = 60.0f;

        [[lyra::range(0.1f, 1000.0f), lyra::label("Size"), lyra::condition("type == ProjectionType::ORTHOGRAPHIC")]]
        float size = 10.0f;

        [[lyra::speed(0.01f), lyra::range(0.1f, 10.0f), lyra::label("Aspect")]]
        float aspect = 1.777f;

        [[lyra::speed(0.01f), lyra::range(0.001f, 1000.0f), lyra::label("Near")]]
        float near_plane = 0.1f;

        [[lyra::speed(1.0f), lyra::range(1.0f, 10000.0f), lyra::label("Far")]]
        float far_plane = 1000.0f;

        [[lyra::hidden]]
        Matrix4x4 projection = Matrix4x4(1.0f);
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_CAMERA_H
