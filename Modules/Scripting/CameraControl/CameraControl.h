#pragma once

#ifndef LYRA_SCRIPTING_CAMERA_CONTROL_H
#define LYRA_SCRIPTING_CAMERA_CONTROL_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Scripting/ScriptTypes.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/Transform.h>

namespace lyra
{
    /**
     * @brief fly first-person camera controller with WASD movement, mouse look, and velocity/rotation damping.
     */
    struct [[lyra::component("FlyCamera", category = "Camera Control", icon = "LYRA_ICON_CAMERA")]] FlyCamera
    {
        [[lyra::speed(0.1f), lyra::range(0.1f, 100.0f)]]
        float move_speed = 10.0f;

        [[lyra::speed(0.1f), lyra::range(1.0f, 10.0f)]]
        float boost_multiplier = 2.5f;

        [[lyra::speed(0.01f), lyra::range(0.01f, 2.0f)]]
        float look_sensitivity = 0.15f;

        [[lyra::speed(0.1f), lyra::range(0.0f, 50.0f)]]
        float move_damping = 10.0f;

        [[lyra::speed(0.1f), lyra::range(0.0f, 50.0f)]]
        float look_damping = 15.0f;

        [[lyra::hidden]]
        float yaw   = 0.0f;

        [[lyra::hidden]]
        float pitch = 0.0f;

        [[lyra::hidden]]
        float target_yaw   = 0.0f;

        [[lyra::hidden]]
        float target_pitch = 0.0f;

        [[lyra::hidden]]
        Vector3 velocity = Vector3(0.0f);
    };

    /**
     * @brief orbit camera controller rotating and zooming around a target pivot point with inertia damping.
     */
    struct [[lyra::component("OrbitCamera", category = "Camera Control", icon = "LYRA_ICON_CAMERA")]] OrbitCamera
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

        [[lyra::speed(0.01f), lyra::range(0.01f, 2.0f)]]
        float look_sensitivity = 0.2f;

        [[lyra::range(0.1f, 20.0f)]]
        float zoom_speed = 2.0f;

        [[lyra::range(0.0f, 50.0f)]]
        float damping = 10.0f;

        bool auto_rotate = false;

        [[lyra::hidden]]
        float yaw   = 0.0f;

        [[lyra::hidden]]
        float pitch = 20.0f;

        [[lyra::hidden]]
        float target_yaw   = 0.0f;

        [[lyra::hidden]]
        float target_pitch = 20.0f;

        [[lyra::hidden]]
        float current_distance = 10.0f;
    };

} // namespace lyra

#endif // LYRA_SCRIPTING_CAMERA_CONTROL_H
