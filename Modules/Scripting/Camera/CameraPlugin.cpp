#include "CameraPlugin.h"
#include "Camera.gen.h"

using namespace lyra;

ScriptAPI lyra::scripts::camera::create()
{
    return generated::create_script_api();
}
