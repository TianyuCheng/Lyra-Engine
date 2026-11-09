#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_CAMERA_H
#define LYRA_ENGINE_COMPONENTS_CAMERA_H

#include <cmath>
#include <algorithm>
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
     * @brief photographic exposure mode.
     */
    enum struct ExposureMode : uint
    {
        MANUAL,          ///< photographic parameters: aperture, shutter speed, ISO
        EV_COMPENSATION, ///< direct EV100 slider + exposure compensation
        AUTO_HISTOGRAM   ///< dynamic GPU eye adaptation via luminance histogram
    };

    /**
     * @brief core camera component with projection parameters and cached projection matrix.
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

    /**
     * @brief physically based photographic exposure and eye adaptation settings component.
     */
    struct [[lyra::component("CameraExposure", category = "Rendering")]] CameraExposure
    {
        [[lyra::label("Exposure Mode")]]
        ExposureMode exposure_mode = ExposureMode::MANUAL;

        [[lyra::range(0.5f, 64.0f), lyra::label("Aperture (f-stop)"), lyra::condition("exposure_mode == ExposureMode::MANUAL")]]
        float aperture = 4.0f; // f/4.0

        [[lyra::range(0.0001f, 10.0f), lyra::label("Shutter Speed (s)"), lyra::condition("exposure_mode == ExposureMode::MANUAL")]]
        float shutter_speed = 1.0f / 125.0f; // 1/125s (8ms)

        [[lyra::range(10.0f, 102400.0f), lyra::label("ISO"), lyra::condition("exposure_mode == ExposureMode::MANUAL")]]
        float iso = 100.0f;

        [[lyra::range(-10.0f, 20.0f), lyra::label("EV100"), lyra::condition("exposure_mode == ExposureMode::EV_COMPENSATION")]]
        float manual_ev100 = 9.7f;

        [[lyra::range(-5.0f, 5.0f), lyra::label("Exposure Compensation")]]
        float exposure_compensation = 0.0f; // EV stops

        // auto-exposure / eye adaptation parameters
        [[lyra::range(-10.0f, 20.0f), lyra::label("Min EV"), lyra::condition("exposure_mode == ExposureMode::AUTO_HISTOGRAM")]]
        float min_ev = -4.0f;

        [[lyra::range(-10.0f, 20.0f), lyra::label("Max EV"), lyra::condition("exposure_mode == ExposureMode::AUTO_HISTOGRAM")]]
        float max_ev = 16.0f;

        [[lyra::range(0.1f, 20.0f), lyra::label("Speed Up"), lyra::condition("exposure_mode == ExposureMode::AUTO_HISTOGRAM")]]
        float speed_up = 3.0f; // adaptation speed towards brighter environments

        [[lyra::range(0.1f, 20.0f), lyra::label("Speed Down"), lyra::condition("exposure_mode == ExposureMode::AUTO_HISTOGRAM")]]
        float speed_down = 1.0f; // adaptation speed towards darker environments

        [[lyra::range(0.0f, 0.5f), lyra::label("Histogram Low %"), lyra::condition("exposure_mode == ExposureMode::AUTO_HISTOGRAM")]]
        float low_percent = 0.10f; // discard bottom 10% darkest pixels

        [[lyra::range(0.5f, 1.0f), lyra::label("Histogram High %"), lyra::condition("exposure_mode == ExposureMode::AUTO_HISTOGRAM")]]
        float high_percent = 0.90f; // discard top 10% brightest pixels
    };

    inline float get_camera_ev100(const CameraExposure& exposure)
    {
        if (exposure.exposure_mode == ExposureMode::EV_COMPENSATION) {
            return exposure.manual_ev100 - exposure.exposure_compensation;
        }
        float ev = std::log2((exposure.aperture * exposure.aperture / std::max(exposure.shutter_speed, 1e-5f)) * (100.0f / std::max(exposure.iso, 1e-5f)));
        return ev - exposure.exposure_compensation;
    }

    inline float get_camera_exposure_scale(const CameraExposure& exposure)
    {
        return 1.0f / (1.2f * std::pow(2.0f, get_camera_ev100(exposure)));
    }

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
        float yaw = 0.0f;

        [[lyra::hidden]]
        float pitch = 0.0f;

        [[lyra::hidden]]
        float target_yaw = 0.0f;

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
        float yaw = 0.0f;

        [[lyra::hidden]]
        float pitch = 20.0f;

        [[lyra::hidden]]
        float target_yaw = 0.0f;

        [[lyra::hidden]]
        float target_pitch = 20.0f;

        [[lyra::hidden]]
        float current_distance = 10.0f;
    };

} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_CAMERA_H
