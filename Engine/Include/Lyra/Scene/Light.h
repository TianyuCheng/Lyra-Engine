#pragma once

#ifndef LYRA_ENGINE_SCENE_LIGHT_H
#define LYRA_ENGINE_SCENE_LIGHT_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>

// reference: `https://google.github.io/filament/Filament.md.html

namespace lyra
{

    // component
    struct PointLight
    {
        Vector3 position;  // x, y, z (12 bytes)
        float   falloff;   // falloff coefficient (4 bytes)
        Vector3 color;     // r, g, b (12 bytes)
        float   intensity; // intensity in watts (4 bytes)
        Vector3 direction; // dx, dy, dz (12 bytes)
        uint    profile;   // IES profile index (4 bytes)
    };

    // component
    struct SpotLight
    {
        Vector3 position;  // x, y, z (12 bytes)
        float   falloff;   // falloff coefficient (4 bytes)
        Vector3 color;     // r, g, b (12 bytes)
        float   intensity; // intensity in watts (4 bytes)
        Vector3 direction; // dx, dy, dz
        uint    profile;   // IES profile index
        Vector2 angle;     // angle scale, angle offset
    };

    // component
    struct DirectionalLight
    {
        Vector3 direction; // dx, dy, dz
        uint    profile;   // IES profile index
        Vector3 color;     // r, g, b
        float   intensity; // intensity in candela
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_LIGHT_H
