#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_AUDIO_H
#define LYRA_ENGINE_COMPONENTS_AUDIO_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Audio/AudioEnums.h>
#include <Lyra/Audio/AudioTypes.h>

namespace lyra
{
    /**
     * @brief Component representing an audio emitter attached to an entity.
     */
    struct [[lyra::component("AudioSource", category = "Audio", icon = "LYRA_ICON_VOLUME_UP")]] AudioSourceComponent
    {
        [[lyra::label("Audio Asset")]]
        RawAssetHandle audio_asset = {};

        [[lyra::label("Bus Channel")]]
        AudioBusType bus_type = AudioBusType::SFX;

        [[lyra::label("Volume"), lyra::range(0.0f, 2.0f)]]
        float volume = 1.0f;

        [[lyra::label("Pitch"), lyra::range(0.1f, 4.0f)]]
        float pitch = 1.0f;

        [[lyra::label("Looping")]]
        bool loop = false;

        [[lyra::label("Play On Awake")]]
        bool play_on_awake = true;

        [[lyra::label("Spatialized (3D)")]]
        bool spatialized = true;

        [[lyra::label("Attenuation Model")]]
        AttenuationModel attenuation_model = AttenuationModel::Inverse;

        [[lyra::label("Min Distance"), lyra::range(0.01f, 1000.0f)]]
        float min_distance = 1.0f;

        [[lyra::label("Max Distance"), lyra::range(0.1f, 10000.0f)]]
        float max_distance = 50.0f;

        [[lyra::label("Rolloff Factor"), lyra::range(0.0f, 10.0f)]]
        float rolloff = 1.0f;

        [[lyra::hidden]]
        SoundHandle runtime_sound = {};
    };

    /**
     * @brief Component representing the listener ears in the 3D scene (typically on the Camera).
     */
    struct [[lyra::component("AudioListener", category = "Audio", icon = "LYRA_ICON_HEADPHONES")]] AudioListenerComponent
    {
        [[lyra::label("Active Listener")]]
        bool is_active = true;

        [[lyra::label("Gain"), lyra::range(0.0f, 2.0f)]]
        float gain = 1.0f;

        [[lyra::label("Listener Index")]]
        uint listener_index = 0;
    };

} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_AUDIO_H
