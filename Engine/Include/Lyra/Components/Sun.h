#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_SUN_H
#define LYRA_ENGINE_COMPONENTS_SUN_H

#include <Lyra/Utilities/Stdint.h>

namespace lyra
{

    // component
    struct [[lyra::component("Sun", category = "Lighting", icon = "LYRA_ICON_SUN")]] Sun
    {
        [[lyra::speed(0.0001f), lyra::range(0.0001f, 0.1f), lyra::label("Angular Radius (rad)")]]
        float angular_radius = 0.004675f; // angular radius in radians (~0.268 degrees, diameter 0.536 degrees)

        [[lyra::speed(0.1f), lyra::range(1.0f, 100.0f), lyra::label("Halo Size")]]
        float halo_size = 10.0f; // halo radius factor relative to sun disk

        [[lyra::speed(1.0f), lyra::range(1.0f, 1000.0f), lyra::label("Halo Falloff")]]
        float halo_falloff = 80.0f; // halo falloff exponent

        [[lyra::speed(0.01f), lyra::range(0.0f, 1.0f), lyra::label("Limb Darkening")]]
        float limb_darkening = 0.6f; // solar limb darkening coefficient
    };

} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_SUN_H
