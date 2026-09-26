#pragma once

#ifndef LYRA_MODULES_SCRIPTING_CAMERA_CONTROL_H
#define LYRA_MODULES_SCRIPTING_CAMERA_CONTROL_H

#include <cmath>
#include <algorithm>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Scene/Transform.h>
#include <Lyra/Scripting/ScriptContext.h>
#include <Lyra/Windowing/WSIEnums.h>

#include "Camera.hxx"

namespace lyra
{
    /**
     * @brief Fly camera update system handling smoothed mouse orientation and keyboard movement.
     */
    [[lyra::system(UPDATE, group = "Camera")]]
    inline void fly_camera(ScriptContext& ctx, FlyCamera& camera, TransformLocal& transform)
    {
        // mouse look while right mouse button is held
        if (ctx.is_mouse_down(MouseButton::RIGHT)) {
            Vector2 delta = ctx.mouse_delta();
            camera.target_yaw -= delta.x * camera.look_sensitivity;
            camera.target_pitch -= delta.y * camera.look_sensitivity;
            camera.target_pitch = std::clamp(camera.target_pitch, -89.0f, 89.0f);
        }

        // smooth rotation damping
        if (camera.look_damping > 0.0f) {
            float rot_t  = 1.0f - std::exp(-camera.look_damping * ctx.dt());
            camera.yaw   = std::lerp(camera.yaw, camera.target_yaw, rot_t);
            camera.pitch = std::lerp(camera.pitch, camera.target_pitch, rot_t);
        } else {
            camera.yaw   = camera.target_yaw;
            camera.pitch = camera.target_pitch;
        }

        Quaternion q_yaw   = glm::angleAxis(glm::radians(camera.yaw), Vector3(0.0f, 1.0f, 0.0f));
        Quaternion q_pitch = glm::angleAxis(glm::radians(camera.pitch), Vector3(1.0f, 0.0f, 0.0f));
        transform.rotation = glm::normalize(q_yaw * q_pitch);
        transform.flags |= TransformFlag::LOCAL_DIRTY;

        // direction vectors from orientation
        Vector3 forward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        Vector3 right   = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        Vector3 up      = Vector3(0.0f, 1.0f, 0.0f);

        Vector3 move_dir(0.0f);
        if (ctx.is_key_down(KeyButton::W)) move_dir += forward;
        if (ctx.is_key_down(KeyButton::S)) move_dir -= forward;
        if (ctx.is_key_down(KeyButton::D)) move_dir += right;
        if (ctx.is_key_down(KeyButton::A)) move_dir -= right;
        if (ctx.is_key_down(KeyButton::SPACE) || ctx.is_key_down(KeyButton::E)) move_dir += up;
        if (ctx.is_key_down(KeyButton::CTRL) || ctx.is_key_down(KeyButton::Q)) move_dir -= up;

        Vector3 target_vel(0.0f);
        if (glm::dot(move_dir, move_dir) > 0.0001f) {
            float speed = camera.move_speed;
            if (ctx.is_key_down(KeyButton::SHIFT)) speed *= camera.boost_multiplier;
            target_vel = glm::normalize(move_dir) * speed;
        }

        // smooth movement velocity damping
        if (camera.move_damping > 0.0f) {
            float move_t    = 1.0f - std::exp(-camera.move_damping * ctx.dt());
            camera.velocity = glm::mix(camera.velocity, target_vel, move_t);
        } else {
            camera.velocity = target_vel;
        }

        if (glm::dot(camera.velocity, camera.velocity) > 0.00001f) {
            ctx.translate(transform, camera.velocity * ctx.dt());
        }
    }

    /**
     * @brief Orbit camera update system handling rotation, auto-rotation, and scroll wheel zoom with damping.
     */
    [[lyra::system(UPDATE, group = "Camera")]]
    inline void orbit_camera(ScriptContext& ctx, OrbitCamera& camera, TransformLocal& transform)
    {
        // mouse drag to orbit around target
        if (ctx.is_mouse_down(MouseButton::RIGHT) || ctx.is_mouse_down(MouseButton::MIDDLE)) {
            Vector2 delta = ctx.mouse_delta();
            camera.target_yaw -= delta.x * camera.look_sensitivity;
            camera.target_pitch += delta.y * camera.look_sensitivity;
            camera.target_pitch = std::clamp(camera.target_pitch, -89.0f, 89.0f);
        }

        // continuous auto rotation
        if (camera.auto_rotate) {
            camera.target_yaw += camera.orbit_speed * ctx.dt();
        }

        // mouse scroll wheel zoom
        Vector2 scroll = ctx.mouse_scroll();
        if (std::abs(scroll.y) > 0.0001f) {
            camera.distance -= scroll.y * camera.zoom_speed;
            camera.distance = std::clamp(camera.distance, camera.min_distance, camera.max_distance);
        }

        // smooth damping interpolation
        if (camera.damping > 0.0f) {
            float t                 = 1.0f - std::exp(-camera.damping * ctx.dt());
            camera.yaw              = std::lerp(camera.yaw, camera.target_yaw, t);
            camera.pitch            = std::lerp(camera.pitch, camera.target_pitch, t);
            camera.current_distance = std::lerp(camera.current_distance, camera.distance, t);
        } else {
            camera.yaw              = camera.target_yaw;
            camera.pitch            = camera.target_pitch;
            camera.current_distance = camera.distance;
        }

        // update transform rotation & position relative to target
        Quaternion q_yaw   = glm::angleAxis(glm::radians(camera.yaw), Vector3(0.0f, 1.0f, 0.0f));
        Quaternion q_pitch = glm::angleAxis(glm::radians(camera.pitch), Vector3(1.0f, 0.0f, 0.0f));
        Quaternion rot     = glm::normalize(q_yaw * q_pitch);

        transform.rotation = rot;
        transform.position = camera.target + Vector3(rot * glm::vec3(0.0f, 0.0f, camera.current_distance));
        transform.flags |= TransformFlag::LOCAL_DIRTY;
    }

    /**
     * @brief Camera projection calculation system updating perspective and orthographic matrices.
     */
    [[lyra::system(UPDATE, group = "Camera")]]
    inline void camera_projection(ScriptContext& ctx, Camera& camera)
    {
        if (camera.type == ProjectionType::PERSPECTIVE) {
            camera.projection = glm::perspective(
                glm::radians(camera.fov), camera.aspect, camera.near_plane, camera.far_plane);
        } else {
            float half_size   = camera.size * 0.5f;
            camera.projection = glm::ortho(
                -half_size * camera.aspect, half_size * camera.aspect,
                -half_size, half_size, camera.near_plane, camera.far_plane);
        }
    }

} // namespace lyra

#endif // LYRA_MODULES_SCRIPTING_CAMERA_CONTROL_H
