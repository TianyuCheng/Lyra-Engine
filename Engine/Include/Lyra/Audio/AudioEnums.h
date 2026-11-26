#pragma once

#ifndef LYRA_ENGINE_AUDIO_AUDIOENUMS_H
#define LYRA_ENGINE_AUDIO_AUDIOENUMS_H

#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    /**
     * @brief Runtime playback state of an audio sound instance.
     */
    enum class AudioPlayState : ubyte
    {
        Playing,
        Paused,
        Stopped,
    };

    /**
     * @brief 3D distance attenuation falloff model.
     */
    enum class AttenuationModel : ubyte
    {
        Inverse,     ///< 1.0 / (1.0 + rolloff * (d - min_d) / (max_d - min_d))
        Linear,      ///< 1.0 - rolloff * (d - min_d) / (max_d - min_d)
        Exponential, ///< pow((d - min_d) / (max_d - min_d), rolloff)
        None,        ///< no distance attenuation
    };

    /**
     * @brief Audio mixing bus channels for grouping and master volume control.
     */
    enum class AudioBusType : ubyte
    {
        Master,
        SFX,
        Music,
        Ambient,
        Voice,
        UI,
        Custom,
    };

    /**
     * @brief Raw PCM sample encoding format.
     */
    enum class AudioSampleFormat : ubyte
    {
        Int16,
        Int24,
        Int32,
        Float32,
    };

    /**
     * @brief Supported audio subsystem plugin backends.
     */
    enum class AudioBackend : ubyte
    {
        Mock,
        Miniaudio,
        Custom,
    };

} // namespace lyra

#endif // LYRA_ENGINE_AUDIO_AUDIOENUMS_H
