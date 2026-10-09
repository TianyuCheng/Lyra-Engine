#pragma once

#ifndef LYRA_ENGINE_SCENE_TRANSFORM_H
#define LYRA_ENGINE_SCENE_TRANSFORM_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Macros.h>

namespace lyra
{
    enum struct TransformFlag : uint
    {
        NONE        = 0x0,
        LOCAL_DIRTY = 0x1,
        WORLD_DIRTY = 0x2,
    };
} // namespace lyra

ENABLE_BIT_FLAGS(lyra::TransformFlag);

namespace lyra
{

    using TransformFlags = BitFlags<TransformFlag>;

    // component
    struct [[lyra::component("Transform", category = "Scene", icon = "LYRA_ICON_TRANSFORM")]] TransformLocal
    {
        [[lyra::label("Position")]]
        Vector3 position = Vector3(0.0f);

        [[lyra::label("Scale"), lyra::reset(1.0f)]]
        Vector3 scale = Vector3(1.0f);

        [[lyra::edit(euler, order = YXZ), lyra::label("Rotation")]]
        Quaternion rotation = glm::identity<Quaternion>();

        [[lyra::hidden]]
        TransformFlags flags = TransformFlag::NONE;
    };

    // component
    struct TransformWorld
    {
        Matrix4x4 xform = Matrix4x4(1.0f);
    };

    /**
     * @brief decompose a 4x4 matrix into position, rotation, and scale.
     */
    FORCE_INLINE void decompose_transform(
        const Matrix4x4& transform,
        Vector3&         out_position,
        Quaternion&      out_rotation,
        Vector3&         out_scale)
    {
        out_position = Vector3(transform[3]);

        out_scale.x = glm::length(Vector3(transform[0]));
        out_scale.y = glm::length(Vector3(transform[1]));
        out_scale.z = glm::length(Vector3(transform[2]));

        if (out_scale.x > 1e-6f && out_scale.y > 1e-6f && out_scale.z > 1e-6f) {
            Matrix3x3 rot_mat(
                Vector3(transform[0]) / out_scale.x,
                Vector3(transform[1]) / out_scale.y,
                Vector3(transform[2]) / out_scale.z);
            out_rotation = glm::quat_cast(rot_mat);
        } else {
            out_rotation = glm::identity<Quaternion>();
        }
    }

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_TRANSFORMH
