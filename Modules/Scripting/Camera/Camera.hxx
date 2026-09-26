#pragma once

#ifndef LYRA_SCENE_CAMERA_H
#define LYRA_SCENE_CAMERA_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Scripting/ScriptTypes.h>

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
        ProjectionType type = ProjectionType::PERSPECTIVE;

        [[lyra::range(1.0f, 179.0f)]]
        float fov = 60.0f;

        [[lyra::range(0.1f, 1000.0f)]]
        float size = 10.0f;

        float aspect = 1.777f;

        [[lyra::range(0.001f, 100.0f)]]
        float near_plane = 0.1f;

        [[lyra::range(1.0f, 10000.0f)]]
        float far_plane = 1000.0f;

        Matrix4x4 projection = Matrix4x4(1.0f);
    };

    /**
     * @brief fly first-person camera controller with WASD movement, mouse look, and velocity/rotation damping.
     */
    struct [[lyra::component("Camera")]] FlyCamera
    {
        [[lyra::range(0.1f, 200.0f)]]
        float move_speed = 10.0f;

        [[lyra::range(1.0f, 10.0f)]]
        float boost_multiplier = 2.5f;

        [[lyra::range(0.01f, 2.0f)]]
        float look_sensitivity = 0.15f;

        [[lyra::range(0.0f, 50.0f)]]
        float move_damping = 10.0f;

        [[lyra::range(0.0f, 50.0f)]]
        float look_damping = 15.0f;

        float yaw   = 0.0f;
        float pitch = 0.0f;

        float target_yaw   = 0.0f;
        float target_pitch = 0.0f;

        Vector3 velocity = Vector3(0.0f);
    };

    /**
     * @brief orbit camera controller rotating and zooming around a target pivot point with inertia damping.
     */
    struct [[lyra::component("Camera")]] OrbitCamera
    {
        Vector3 target = Vector3(0.0f);

        [[lyra::range(0.5f, 500.0f)]]
        float distance = 10.0f;

        [[lyra::range(0.1f, 50.0f)]]
        float min_distance = 1.0f;

        [[lyra::range(10.0f, 1000.0f)]]
        float max_distance = 100.0f;

        [[lyra::range(0.0f, 360.0f)]]
        float orbit_speed = 45.0f;

        [[lyra::range(0.01f, 2.0f)]]
        float look_sensitivity = 0.2f;

        [[lyra::range(0.1f, 20.0f)]]
        float zoom_speed = 2.0f;

        [[lyra::range(0.0f, 50.0f)]]
        float damping = 10.0f;

        bool auto_rotate = false;

        float yaw   = 0.0f;
        float pitch = 20.0f;

        float target_yaw   = 0.0f;
        float target_pitch = 20.0f;

        float current_distance = 10.0f;
    };

} // namespace lyra

#endif // LYRA_SCENE_CAMERA_H
