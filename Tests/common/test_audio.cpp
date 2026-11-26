#include <doctest/doctest.h>
#include <Lyra/Audio/AudioAPI.h>
#include <Lyra/Audio/AudioDescs.h>
#include <Lyra/Audio/AudioEnums.h>
#include <Lyra/Audio/AudioTypes.h>

using namespace lyra;

TEST_CASE("Audio Subsystem - Mock Backend Lifecycle and Playback")
{
    // 1. Subsystem initialization
    bool init_ok = Audio::init(AudioBackend::Mock);
    CHECK(init_ok);
    CHECK(Audio::api() != nullptr);
    CHECK(Audio::get_active_backend() == AudioBackend::Mock);

    // 2. Master volume control
    Audio::set_master_volume(0.75f);
    CHECK(Audio::get_master_volume() == doctest::Approx(0.75f));

    // 3. Audio Bus creation and properties
    AudioBusDescriptor bus_desc{};
    bus_desc.name    = "SFX_Bus";
    bus_desc.volume  = 0.8f;
    bus_desc.muted   = false;
    AudioBus sfx_bus = Audio::create_bus(bus_desc);
    CHECK(sfx_bus.valid());
    CHECK(sfx_bus.get_volume() == doctest::Approx(0.8f));
    CHECK(!sfx_bus.is_muted());

    sfx_bus.set_muted(true);
    CHECK(sfx_bus.is_muted());

    // 4. Clip creation
    AudioClipDescriptor clip_desc{};
    clip_desc.source_format = "wav";
    AudioClip clip          = Audio::create_clip(clip_desc);
    CHECK(clip.valid());
    CHECK(clip.get_duration() == doctest::Approx(5.0f));

    // 5. Sound creation and playback state
    SoundDescriptor sound_desc{};
    sound_desc.clip        = clip.handle;
    sound_desc.bus         = sfx_bus.handle;
    sound_desc.volume      = 1.0f;
    sound_desc.pitch       = 1.0f;
    sound_desc.looping     = false;
    sound_desc.spatialized = true;
    sound_desc.position    = Vector3(10.0f, 0.0f, 5.0f);

    Sound sound = Audio::create_sound(sound_desc);
    CHECK(sound.valid());
    CHECK(sound.get_state() == AudioPlayState::Stopped);
    CHECK(sound.get_cursor() == doctest::Approx(0.0f));

    sound.play();
    CHECK(sound.get_state() == AudioPlayState::Playing);

    // 6. Update simulation tick
    Audio::update(1.5f);
    CHECK(sound.get_cursor() == doctest::Approx(1.5f));
    CHECK(sound.get_state() == AudioPlayState::Playing);

    sound.pause();
    CHECK(sound.get_state() == AudioPlayState::Paused);

    Audio::update(1.0f);
    CHECK(sound.get_cursor() == doctest::Approx(1.5f)); // paused cursor does not advance

    sound.seek(3.0f);
    CHECK(sound.get_cursor() == doctest::Approx(3.0f));

    sound.stop();
    CHECK(sound.get_state() == AudioPlayState::Stopped);
    CHECK(sound.get_cursor() == doctest::Approx(0.0f));

    // 7. 3D Listener state
    AudioListenerDescriptor listener{};
    listener.position = Vector3(0.0f, 1.7f, 0.0f);
    listener.forward  = Vector3(0.0f, 0.0f, -1.0f);
    listener.up       = Vector3(0.0f, 1.0f, 0.0f);
    listener.gain     = 1.0f;
    Audio::set_listener(0, listener);

    // 8. One-shot playback convenience
    Audio::play_one_shot(clip.handle, Vector3(5.0f, 0.0f, 0.0f), 0.9f);

    // 9. Resource cleanup and shutdown
    sound.destroy();
    CHECK(!sound.valid());

    clip.destroy();
    CHECK(!clip.valid());

    sfx_bus.destroy();
    CHECK(!sfx_bus.valid());

    Audio::shutdown();
    CHECK(Audio::api() == nullptr);
}

TEST_CASE("Audio Subsystem - Miniaudio Backend Lifecycle")
{
    bool init_ok = Audio::init(AudioBackend::Miniaudio);
    if (!init_ok) {
        MESSAGE("Miniaudio backend init skipped (no audio device available)");
        return;
    }

    CHECK(Audio::api() != nullptr);
    CHECK(Audio::get_active_backend() == AudioBackend::Miniaudio);

    Audio::set_master_volume(0.6f);
    CHECK(Audio::get_master_volume() == doctest::Approx(0.6f));

    AudioBusDescriptor bus_desc{};
    bus_desc.name      = "MusicBus";
    bus_desc.volume    = 0.5f;
    bus_desc.muted     = false;
    AudioBus music_bus = Audio::create_bus(bus_desc);
    CHECK(music_bus.valid());
    CHECK(music_bus.get_volume() == doctest::Approx(0.5f));
    CHECK(!music_bus.is_muted());

    music_bus.set_muted(true);
    CHECK(music_bus.is_muted());

    AudioListenerDescriptor listener{};
    listener.position = Vector3(0.0f, 1.0f, 0.0f);
    listener.forward  = Vector3(0.0f, 0.0f, -1.0f);
    listener.up       = Vector3(0.0f, 1.0f, 0.0f);
    listener.gain     = 1.0f;
    Audio::set_listener(0, listener);

    Audio::update(0.016f);

    music_bus.destroy();
    CHECK(!music_bus.valid());

    Audio::shutdown();
    CHECK(Audio::api() == nullptr);
}
