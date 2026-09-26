#pragma once

#ifndef LYRA_MODULES_SCRIPTING_CAMERA_PLUGIN_H
#define LYRA_MODULES_SCRIPTING_CAMERA_PLUGIN_H

#include <Lyra/Scripting/ScriptTypes.h>

namespace lyra::scripts::camera
{
    auto create() -> ScriptAPI;
} // namespace lyra::scripts::camera

#endif // LYRA_MODULES_SCRIPTING_CAMERA_PLUGIN_H
