#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_CLOUDSCAPE_H
#define LYRA_ENGINE_COMPONENTS_CLOUDSCAPE_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>

namespace lyra
{
    /**
     * @brief ECS Component declaring physical volumetric cloudscapes in the environment.
     * Integrates multi-octave Perlin-Worley density modeling, dual-lobe Henyey-Greenstein
     * forward silver-lining phase function, Beer-Powder dark border crevasse shading,
     * and multiple scattering approximation boost.
     */
    struct [[lyra::component("Cloudscape", category = "Environment", icon = "LYRA_ICON_CLOUD")]] Cloudscape
    {
        [[lyra::speed(0.1f), lyra::range(0.1f, 20.0f), lyra::label("Bottom Altitude (km)")]]
        float bottom_altitude = 1.5f; // inner boundary altitude in km

        [[lyra::speed(0.1f), lyra::range(0.2f, 30.0f), lyra::label("Top Altitude (km)")]]
        float top_altitude = 4.0f; // outer boundary altitude in km

        [[lyra::speed(0.01f), lyra::range(0.0f, 1.0f), lyra::label("Coverage")]]
        float coverage = 0.48f; // global cloud coverage fraction in [0, 1]

        [[lyra::speed(0.005f), lyra::range(0.001f, 5.0f), lyra::label("Density Scale")]]
        float density_scale = 0.55f; // volumetric extinction multiplier

        [[lyra::speed(0.01f), lyra::range(0.0f, 1.0f), lyra::label("Cloud Type")]]
        float cloud_type = 0.7f; // 0=Stratus, 0.5=Stratocumulus, 1=Cumulus

        [[lyra::speed(0.01f), lyra::range(0.01f, 5.0f), lyra::label("Shape Scale")]]
        float shape_scale = 0.35f; // base low-frequency noise frequency

        [[lyra::speed(0.05f), lyra::range(0.1f, 20.0f), lyra::label("Detail Scale")]]
        float detail_scale = 2.8f; // high-frequency erosion frequency

        [[lyra::speed(0.01f), lyra::range(0.0f, 1.0f), lyra::label("Detail Strength")]]
        float detail_strength = 0.35f; // erosion influence in [0, 1]

        [[lyra::speed(0.01f), lyra::range(0.0f, 1.0f), lyra::label("Curl Strength")]]
        float curl_strength = 0.15f; // turbulence curl distortion weight

        Vector3 wind_direction = Vector3(1.0f, 0.0f, 0.4f); // normalized horizontal wind
        float   wind_speed     = 0.04f;                     // wind propagation speed in km/s
        float   height_shear   = 0.3f;                      // altitude-dependent wind shear

        [[lyra::speed(0.01f), lyra::range(0.0f, 2.0f), lyra::label("Multi-Scatter Boost")]]
        float multi_scatter_boost = 0.5f; // multiple scattering energy transmission factor

        [[lyra::speed(0.05f), lyra::range(0.0f, 5.0f), lyra::label("Powder Scale")]]
        float powder_scale = 1.5f; // Beer-Powder dark border factor

        [[lyra::speed(0.01f), lyra::range(0.0f, 1.0f), lyra::label("Forward Phase (g1)")]]
        float phase_g_forward = 0.82f; // forward Mie asymmetry parameter

        [[lyra::speed(0.01f), lyra::range(-1.0f, 0.0f), lyra::label("Backward Phase (g2)")]]
        float phase_g_backward = -0.22f; // backward Mie asymmetry parameter

        [[lyra::speed(0.01f), lyra::range(0.0f, 1.0f), lyra::label("Phase Blend")]]
        float phase_blend = 0.72f; // dual-lobe blend weight

        uint max_steps = 32; // raymarching step count
    };
} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_CLOUDSCAPE_H
