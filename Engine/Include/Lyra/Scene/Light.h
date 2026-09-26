#pragma once

#ifndef LYRA_ENGINE_SCENE_LIGHT_H
#define LYRA_ENGINE_SCENE_LIGHT_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>

// reference: `https://google.github.io/filament/Filament.md.html

namespace lyra
{

    // component
    struct [[lyra::component("Point Light", category = "Lighting", icon = "LYRA_ICON_LIGHT")]] PointLight
    {
        [[lyra::speed(0.1f), lyra::label("Position")]]
        Vector3 position = Vector3(0.0f); // x, y, z (12 bytes)

        [[lyra::label("Color")]]
        Vector3 color = Vector3(1.0f); // r, g, b (12 bytes)

        [[lyra::speed(0.01f), lyra::label("Direction")]]
        Vector3 direction = Vector3(0.0f, -1.0f, 0.0f); // dx, dy, dz (12 bytes)

        [[lyra::range(0.0f, 1000.0f), lyra::label("Falloff")]]
        float falloff = 1.0f; // falloff coefficient (4 bytes)

        [[lyra::range(0.0f, 100000.0f), lyra::label("Intensity")]]
        float intensity = 100.0f; // intensity in watts (4 bytes)

        [[lyra::label("Profile")]]
        uint profile = 0; // IES profile index (4 bytes)
    };

    // component
    struct [[lyra::component("Spot Light", category = "Lighting", icon = "LYRA_ICON_LIGHT")]] SpotLight
    {
        [[lyra::speed(0.1f), lyra::label("Position")]]
        Vector3 position = Vector3(0.0f); // x, y, z (12 bytes)

        [[lyra::label("Color")]]
        Vector3 color = Vector3(1.0f); // r, g, b (12 bytes)

        [[lyra::speed(0.01f), lyra::label("Direction")]]
        Vector3 direction = Vector3(0.0f, -1.0f, 0.0f); // dx, dy, dz

        [[lyra::label("Angle")]]
        Vector2 angle = Vector2(0.0f); // angle scale, angle offset

        [[lyra::range(0.0f, 1000.0f), lyra::label("Falloff")]]
        float falloff = 1.0f; // falloff coefficient (4 bytes)

        [[lyra::range(0.0f, 100000.0f), lyra::label("Intensity")]]
        float intensity = 100.0f; // intensity in watts (4 bytes)

        [[lyra::label("Profile")]]
        uint profile = 0; // IES profile index
    };

    // component
    struct [[lyra::component("Directional Light", category = "Lighting", icon = "LYRA_ICON_SUN")]] DirectionalLight
    {
        [[lyra::speed(0.01f), lyra::label("Direction")]]
        Vector3 direction = Vector3(0.0f, -1.0f, 0.0f); // dx, dy, dz

        [[lyra::label("Color")]]
        Vector3 color = Vector3(1.0f); // r, g, b

        [[lyra::range(0.0f, 100000.0f), lyra::label("Intensity")]]
        float intensity = 1000.0f; // intensity in candela

        [[lyra::label("Profile")]]
        uint profile = 0; // IES profile index
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_LIGHT_H
