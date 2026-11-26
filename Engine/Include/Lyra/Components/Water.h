#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_WATER_H
#define LYRA_ENGINE_COMPONENTS_WATER_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Rendering/Passes/OceanPass.h>

namespace lyra
{
    /**
     * @brief ECS Component declaring an agitated water or ocean surface.
     */
    struct OceanSurface
    {
        float                           water_depth   = 15.0f;
        float                           water_clarity = 1.0f;
        float                           wave_speed    = 1.0f;
        float                           roughness     = 0.06f;
        float                           foam_scale    = 1.0f;
        Vector4                         deep_color    = Vector4(0.012f, 0.055f, 0.13f, 1.0f);
        Vector4                         shallow_color = Vector4(0.045f, 0.28f, 0.36f, 1.0f);
        Array<passes::OceanWaveData, 4> waves;
    };
} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_WATER_H
