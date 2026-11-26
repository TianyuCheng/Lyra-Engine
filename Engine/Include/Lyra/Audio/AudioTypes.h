#pragma once

#ifndef LYRA_ENGINE_AUDIO_AUDIOTYPES_H
#define LYRA_ENGINE_AUDIO_AUDIOTYPES_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Audio/AudioEnums.h>

namespace lyra
{
    // forward declarations
    struct AudioDeviceDescriptor;
    struct AudioClipDescriptor;
    struct SoundDescriptor;
    struct AudioListenerDescriptor;
    struct AudioBusDescriptor;
    struct AudioAPI;

    // handle tags
    struct AudioClipTag {};
    struct SoundTag {};
    struct AudioBusTag {};

    /**
     * @brief Type-safe generation handle wrapper for audio resources.
     */
    template <typename Tag>
    struct AudioHandle
    {
        uint value = 0;

        constexpr AudioHandle() = default;
        constexpr explicit AudioHandle(uint val) : value(val) {}

        [[nodiscard]] constexpr bool valid() const { return value != 0; }
        constexpr void reset() { value = 0; }

        constexpr bool operator==(const AudioHandle& other) const = default;
        constexpr bool operator!=(const AudioHandle& other) const = default;
    };

    using AudioClipHandle = AudioHandle<AudioClipTag>;
    using SoundHandle     = AudioHandle<SoundTag>;
    using AudioBusHandle  = AudioHandle<AudioBusTag>;

    /**
     * @brief High-level fluent wrapper around a sound instance.
     */
    struct Sound
    {
        SoundHandle handle;

        FORCE_INLINE      operator SoundHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        void play();
        void pause();
        void stop();
        void seek(float seconds);
        auto get_cursor() const -> float;
        auto get_state() const -> AudioPlayState;

        void set_volume(float volume);
        auto get_volume() const -> float;
        void set_pitch(float pitch);
        auto get_pitch() const -> float;
        void set_looping(bool looping);
        bool is_looping() const;

        void set_position(const Vector3& pos);
        void set_velocity(const Vector3& vel);
        void set_direction(const Vector3& dir);
        void destroy();
    };

    /**
     * @brief High-level wrapper around an audio clip buffer.
     */
    struct AudioClip
    {
        AudioClipHandle handle;

        FORCE_INLINE      operator AudioClipHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        auto get_duration() const -> float;
        void destroy();
    };

    /**
     * @brief High-level wrapper around an audio mixing bus channel.
     */
    struct AudioBus
    {
        AudioBusHandle handle;

        FORCE_INLINE      operator AudioBusHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        void set_volume(float volume);
        auto get_volume() const -> float;
        void set_muted(bool muted);
        bool is_muted() const;
        void destroy();
    };

    /**
     * @brief Global Audio subsystem coordinator and plugin manager.
     */
    struct Audio
    {
        static bool init(AudioBackend backend = AudioBackend::Miniaudio);
        static bool init(StringView custom_backend);
        static void shutdown();

        static auto api() -> AudioAPI*;
        static auto get_active_backend() -> AudioBackend;

        static void update(float delta_time);
        static void set_master_volume(float volume);
        static auto get_master_volume() -> float;

        static auto create_clip(const AudioClipDescriptor& desc) -> AudioClip;
        static auto create_sound(const SoundDescriptor& desc) -> Sound;
        static auto create_bus(const AudioBusDescriptor& desc) -> AudioBus;
        static void set_listener(uint index, const AudioListenerDescriptor& desc);
        static void play_one_shot(AudioClipHandle clip, const Vector3& pos = Vector3(0.0f), float volume = 1.0f);
    };

} // namespace lyra

#endif // LYRA_ENGINE_AUDIO_AUDIOTYPES_H
