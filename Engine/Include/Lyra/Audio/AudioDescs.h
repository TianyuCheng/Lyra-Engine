#pragma once

#ifndef LYRA_ENGINE_AUDIO_AUDIODESCS_H
#define LYRA_ENGINE_AUDIO_AUDIODESCS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Audio/AudioEnums.h>

namespace lyra
{
    // forward declarations
    template <typename Tag>
    struct AudioHandle;
    struct AudioClipTag;
    struct AudioBusTag;
    using AudioClipHandle = AudioHandle<AudioClipTag>;
    using AudioBusHandle  = AudioHandle<AudioBusTag>;

    /**
     * @brief Subsystem and output device initialization parameters.
     */
    struct AudioDeviceDescriptor
    {
        uint  sample_rate        = 48000; ///< primary hardware sample rate.
        uint  channels           = 2;     ///< master output channel count (2 = Stereo, 6 = 5.1).
        uint  buffer_frame_size  = 1024;  ///< audio mix buffer period frame count.
        float master_volume      = 1.0f;  ///< initial global master gain in [0, 1].
        bool  enable_spatializer = true;  ///< enable 3D positional and panning processing.
    };

    /**
     * @brief Parameters for loading an audio sound buffer / clip.
     */
    struct AudioClipDescriptor
    {
        Span<ubyte> data;                     ///< encoded file payload or raw PCM buffer.
        CString     source_format    = "";    ///< "wav", "ogg", "mp3", "flac" or empty for raw PCM.
        bool        stream_from_disk = false; ///< stream large files (e.g. music) from disk without full preloading.
        StringView  stream_path      = {};    ///< virtual filesystem path when stream_from_disk is true.
    };

    /**
     * @brief Construction properties for creating a playable sound instance.
     */
    struct SoundDescriptor
    {
        AudioClipHandle  clip;
        AudioBusHandle   bus;
        float            volume            = 1.0f;
        float            pitch             = 1.0f;
        bool             looping           = false;
        bool             spatialized       = true;
        AttenuationModel attenuation_model = AttenuationModel::Inverse;
        float            min_distance      = 1.0f;
        float            max_distance      = 50.0f;
        float            rolloff           = 1.0f;
        float            inner_cone_angle  = glm::radians(360.0f);
        float            outer_cone_angle  = glm::radians(360.0f);
        float            outer_cone_gain   = 0.0f;
        Vector3          position          = Vector3(0.0f);
        Vector3          velocity          = Vector3(0.0f);
        Vector3          direction         = Vector3(0.0f, 0.0f, -1.0f);
    };

    /**
     * @brief Properties for a 3D audio listener (ears of the player/camera).
     */
    struct AudioListenerDescriptor
    {
        Vector3 position = Vector3(0.0f);
        Vector3 velocity = Vector3(0.0f);
        Vector3 forward  = Vector3(0.0f, 0.0f, -1.0f);
        Vector3 up       = Vector3(0.0f, 1.0f, 0.0f);
        float   gain     = 1.0f;
    };

    /**
     * @brief Configuration for a mixing bus channel.
     */
    struct AudioBusDescriptor
    {
        StringView     name   = "Bus";
        AudioBusHandle parent = {};
        float          volume = 1.0f;
        bool           muted  = false;
    };

} // namespace lyra

#endif // LYRA_ENGINE_AUDIO_AUDIODESCS_H
