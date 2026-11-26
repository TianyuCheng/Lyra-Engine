#pragma once

#ifndef LYRA_ENGINE_AUDIO_AUDIOAPI_H
#define LYRA_ENGINE_AUDIO_AUDIOAPI_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Audio/AudioTypes.h>
#include <Lyra/Audio/AudioDescs.h>
#include <Lyra/Audio/AudioEnums.h>

namespace lyra
{
    /**
     * @brief C-ABI compatible interface table for swappable audio engine plugins.
     */
    struct AudioAPI
    {
        // Identification
        CString (*get_api_name)();

        // Subsystem Device Lifecycle
        bool (*create_device)(const AudioDeviceDescriptor& desc);
        void (*delete_device)();
        void (*update)(float delta_time);
        void (*set_master_volume)(float volume);
        auto (*get_master_volume)() -> float;

        // Clip Management
        bool (*create_clip)(const AudioClipDescriptor& desc, AudioClipHandle& out_clip);
        void (*delete_clip)(AudioClipHandle clip);
        auto (*get_clip_duration)(AudioClipHandle clip) -> float;

        // Bus Mixing Hierarchy
        bool (*create_bus)(const AudioBusDescriptor& desc, AudioBusHandle& out_bus);
        void (*delete_bus)(AudioBusHandle bus);
        void (*set_bus_volume)(AudioBusHandle bus, float volume);
        auto (*get_bus_volume)(AudioBusHandle bus) -> float;
        void (*set_bus_muted)(AudioBusHandle bus, bool muted);
        bool (*is_bus_muted)(AudioBusHandle bus);

        // Sound Instances & Playback
        bool (*create_sound)(const SoundDescriptor& desc, SoundHandle& out_sound);
        void (*delete_sound)(SoundHandle sound);
        void (*play)(SoundHandle sound);
        void (*pause)(SoundHandle sound);
        void (*stop)(SoundHandle sound);
        void (*seek)(SoundHandle sound, float seconds);
        auto (*get_cursor)(SoundHandle sound) -> float;
        auto (*get_play_state)(SoundHandle sound) -> AudioPlayState;

        // Sound Properties
        void (*set_volume)(SoundHandle sound, float volume);
        auto (*get_volume)(SoundHandle sound) -> float;
        void (*set_pitch)(SoundHandle sound, float pitch);
        auto (*get_pitch)(SoundHandle sound) -> float;
        void (*set_looping)(SoundHandle sound, bool looping);
        bool (*is_looping)(SoundHandle sound);

        // 3D Spatial Audio Attributes
        void (*set_spatialization_enabled)(SoundHandle sound, bool enabled);
        void (*set_position)(SoundHandle sound, const Vector3& position);
        void (*set_velocity)(SoundHandle sound, const Vector3& velocity);
        void (*set_direction)(SoundHandle sound, const Vector3& direction);
        void (*set_attenuation)(SoundHandle sound, AttenuationModel model, float min_dist, float max_dist, float rolloff);
        void (*set_cone)(SoundHandle sound, float inner_angle, float outer_angle, float outer_gain);

        // Listener
        void (*set_listener_state)(uint index, const AudioListenerDescriptor& desc);

        // One-Shot Sound Convenience Dispatch
        void (*play_one_shot)(AudioClipHandle clip, AudioBusHandle bus, float volume, float pitch, const Vector3& position);
    };

} // namespace lyra

#endif // LYRA_ENGINE_AUDIO_AUDIOAPI_H
