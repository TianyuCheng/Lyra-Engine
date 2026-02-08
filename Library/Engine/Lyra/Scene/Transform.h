#pragma once

#ifndef LYRA_LIBRARY_SCENE_TRANSFORM_H
#define LYRA_LIBRARY_SCENE_TRANSFORM_H

#include <Lyra/Common/Math.h>

namespace lyra
{

    struct Transform
    {
        Vector3    scale    = Vector3(1.0f);
        Vector3    position = Vector3(0.0f);
        Quaternion rotation = Quaternion();
    };

    struct WorldTransform
    {
        Matrix4x4 xform = Matrix4x4();

        bool dirty = false;
    };

    struct CameraTransform
    {
        Matrix4x4 projection;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_SCENE_TRANSFORM_H
