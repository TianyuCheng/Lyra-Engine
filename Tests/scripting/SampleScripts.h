#pragma once

#ifndef LYRA_SAMPLE_SCRIPTS_H
#define LYRA_SAMPLE_SCRIPTS_H

#include <Lyra/Scene/Transform.h>
#include <Lyra/Scripting/ScriptContext.h>

struct [[lyra::component("Camera")]] OrbitCamera
{
    lyra::Vector3 axis = lyra::Vector3(0.0f, 1.0f, 0.0f);

    [[lyra::range(0, 360), lyra::tooltip("degrees per second")]]
    float speed = 30.0f;
};

[[lyra::system(UPDATE, group = "Gameplay/Camera")]]
inline void orbit(lyra::ScriptContext& ctx, OrbitCamera& cam, lyra::TransformLocal& transform)
{
    ctx.rotate(transform, cam.axis, ctx.dt() * cam.speed);
}

#endif // LYRA_SAMPLE_SCRIPTS_H
