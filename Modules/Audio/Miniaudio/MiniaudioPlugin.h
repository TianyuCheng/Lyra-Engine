#pragma once

#ifndef LYRA_MODULES_AUDIO_MINIAUDIO_MINIAUDIOPLUGIN_H
#define LYRA_MODULES_AUDIO_MINIAUDIO_MINIAUDIOPLUGIN_H

#include <Lyra/Audio/AudioAPI.h>

namespace lyra::miniaudio
{
    void prepare();
    void cleanup();
    auto create() -> AudioAPI;

    CString        get_api_name();
    bool           create_device(const AudioDeviceDescriptor& desc);
    void           delete_device();
    void           update(float delta_time);
    void           set_master_volume(float volume);
    float          get_master_volume();
    bool           create_clip(const AudioClipDescriptor& desc, AudioClipHandle& out_clip);
    void           delete_clip(AudioClipHandle clip);
    float          get_clip_duration(AudioClipHandle clip);
    bool           create_bus(const AudioBusDescriptor& desc, AudioBusHandle& out_bus);
    void           delete_bus(AudioBusHandle bus);
    void           set_bus_volume(AudioBusHandle bus, float volume);
    float          get_bus_volume(AudioBusHandle bus);
    void           set_bus_muted(AudioBusHandle bus, bool muted);
    bool           is_bus_muted(AudioBusHandle bus);
    bool           create_sound(const SoundDescriptor& desc, SoundHandle& out_sound);
    void           delete_sound(SoundHandle sound);
    void           play(SoundHandle sound);
    void           pause(SoundHandle sound);
    void           stop(SoundHandle sound);
    void           seek(SoundHandle sound, float seconds);
    float          get_cursor(SoundHandle sound);
    AudioPlayState get_play_state(SoundHandle sound);
    void           set_volume(SoundHandle sound, float volume);
    float          get_volume(SoundHandle sound);
    void           set_pitch(SoundHandle sound, float pitch);
    float          get_pitch(SoundHandle sound);
    void           set_looping(SoundHandle sound, bool looping);
    bool           is_looping(SoundHandle sound);
    void           set_spatialization_enabled(SoundHandle sound, bool enabled);
    void           set_position(SoundHandle sound, const Vector3& position);
    void           set_velocity(SoundHandle sound, const Vector3& velocity);
    void           set_direction(SoundHandle sound, const Vector3& direction);
    void           set_attenuation(SoundHandle sound, AttenuationModel model, float min_dist, float max_dist, float rolloff);
    void           set_cone(SoundHandle sound, float inner_angle, float outer_angle, float outer_gain);
    void           set_listener_state(uint index, const AudioListenerDescriptor& desc);
    void           play_one_shot(AudioClipHandle clip, AudioBusHandle bus, float volume, float pitch, const Vector3& position);
} // namespace lyra::miniaudio

#endif // LYRA_MODULES_AUDIO_MINIAUDIO_MINIAUDIOPLUGIN_H
