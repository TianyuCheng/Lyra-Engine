#pragma once

#ifndef LYRA_ENGINE_SCENE_TRANSFORM_H
#define LYRA_ENGINE_SCENE_TRANSFORM_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/BitFlags.h>

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
    struct TransformLocal
    {
        Quaternion     rotation = glm::identity<Quaternion>();
        Vector3        position = Vector3(0.0f);
        Vector3        scale    = Vector3(1.0f);
        TransformFlags flags    = TransformFlag::NONE;
    };

    // component
    struct TransformWorld
    {
        Matrix4x4 xform = Matrix4x4();
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_TRANSFORMH
