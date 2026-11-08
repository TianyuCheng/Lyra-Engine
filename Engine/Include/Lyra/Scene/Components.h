#pragma once

#ifndef LYRA_ENGINE_SCENE_COMPONENTS_H
#define LYRA_ENGINE_SCENE_COMPONENTS_H

#include <Lyra/Scene/Mesh.h>
#include <Lyra/Scene/Light.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/Transform.h>
#include <Lyra/Scripting/ScriptTypes.h>

namespace lyra::scripts::components
{
    auto create() -> ScriptAPI;
} // namespace lyra::scripts::components

#endif // LYRA_ENGINE_SCENE_COMPONENTS_H
