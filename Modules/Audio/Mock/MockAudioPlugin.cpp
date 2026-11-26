#include "MockAudioPlugin.h"
#include <Lyra/Audio/AudioDescs.h>
#include <Lyra/Audio/AudioEnums.h>
#include <Lyra/Audio/AudioTypes.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Sync.h>

using namespace lyra;

static Atomic<uint> g_handle_counter{1};

template <typename T>
static auto make_mock_handle() -> T
{
    uint id = g_handle_counter.fetch_add(1);
    return T(id);
}

static auto get_logger() -> Logger
{
    static Logger LOGGER = create_logger("MockAudio", LogLevel::info);
    return LOGGER;
}

struct MockClipData
{
    float duration_seconds = 5.0f;
    bool  is_streamed      = false;
};

struct MockBusData
{
    StringView     name   = "Bus";
    AudioBusHandle parent = {};
    float          volume = 1.0f;
    bool           muted  = false;
};

struct MockSoundData
{
    SoundDescriptor desc;
    AudioPlayState  state     = AudioPlayState::Stopped;
    float           cursor    = 0.0f;
    float           volume    = 1.0f;
    float           pitch     = 1.0f;
    bool            looping   = false;
    bool            spatial   = true;
    Vector3         position  = Vector3(0.0f);
    Vector3         velocity  = Vector3(0.0f);
    Vector3         direction = Vector3(0.0f, 0.0f, -1.0f);
};

struct MockAudioState
{
    AudioDeviceDescriptor        device_desc{};
    float                        master_volume = 1.0f;
    HashMap<uint, MockClipData>  clips;
    HashMap<uint, MockBusData>   buses;
    HashMap<uint, MockSoundData> sounds;
    AudioListenerDescriptor      listeners[4]{};
    bool                         active = false;
};

static Mutex          g_state_mutex;
static MockAudioState g_state;

CString lyra::mockaudio::get_api_name()
{
    return "MockAudio";
}

bool lyra::mockaudio::create_device(const AudioDeviceDescriptor& desc)
{
    LockGuard<Mutex> lock(g_state_mutex);
    g_state.device_desc   = desc;
    g_state.master_volume = desc.master_volume;
    g_state.active        = true;
    return true;
}

void lyra::mockaudio::delete_device()
{
    LockGuard<Mutex> lock(g_state_mutex);
    g_state.clips.clear();
    g_state.buses.clear();
    g_state.sounds.clear();
    g_state.active = false;
}

void lyra::mockaudio::update(float delta_time)
{
    LockGuard<Mutex> lock(g_state_mutex);
    if (!g_state.active) return;

    for (auto& [id, sound] : g_state.sounds) {
        if (sound.state != AudioPlayState::Playing) continue;

        float duration = 5.0f;
        auto  clip_it  = g_state.clips.find(sound.desc.clip.value);
        if (clip_it != g_state.clips.end()) {
            duration = clip_it->second.duration_seconds;
        }

        sound.cursor += delta_time * sound.pitch;
        if (sound.cursor >= duration) {
            if (sound.looping) {
                sound.cursor = std::fmod(sound.cursor, duration);
            } else {
                sound.cursor = duration;
                sound.state  = AudioPlayState::Stopped;
            }
        }
    }
}

void lyra::mockaudio::set_master_volume(float volume)
{
    LockGuard<Mutex> lock(g_state_mutex);
    g_state.master_volume = glm::clamp(volume, 0.0f, 2.0f);
}

float lyra::mockaudio::get_master_volume()
{
    LockGuard<Mutex> lock(g_state_mutex);
    return g_state.master_volume;
}

bool lyra::mockaudio::create_clip(const AudioClipDescriptor& desc, AudioClipHandle& out_clip)
{
    LockGuard<Mutex> lock(g_state_mutex);
    out_clip = make_mock_handle<AudioClipHandle>();

    MockClipData clip{};
    clip.is_streamed              = desc.stream_from_disk;
    clip.duration_seconds         = 5.0f; // mock default 5.0s clip
    g_state.clips[out_clip.value] = clip;
    return true;
}

void lyra::mockaudio::delete_clip(AudioClipHandle clip)
{
    LockGuard<Mutex> lock(g_state_mutex);
    g_state.clips.erase(clip.value);
}

float lyra::mockaudio::get_clip_duration(AudioClipHandle clip)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.clips.find(clip.value);
    if (it != g_state.clips.end()) {
        return it->second.duration_seconds;
    }
    return 0.0f;
}

bool lyra::mockaudio::create_bus(const AudioBusDescriptor& desc, AudioBusHandle& out_bus)
{
    LockGuard<Mutex> lock(g_state_mutex);
    out_bus = make_mock_handle<AudioBusHandle>();

    MockBusData bus{};
    bus.name                     = desc.name;
    bus.parent                   = desc.parent;
    bus.volume                   = desc.volume;
    bus.muted                    = desc.muted;
    g_state.buses[out_bus.value] = bus;
    return true;
}

void lyra::mockaudio::delete_bus(AudioBusHandle bus)
{
    LockGuard<Mutex> lock(g_state_mutex);
    g_state.buses.erase(bus.value);
}

void lyra::mockaudio::set_bus_volume(AudioBusHandle bus, float volume)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.buses.find(bus.value);
    if (it != g_state.buses.end()) {
        it->second.volume = glm::clamp(volume, 0.0f, 2.0f);
    }
}

float lyra::mockaudio::get_bus_volume(AudioBusHandle bus)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.buses.find(bus.value);
    if (it != g_state.buses.end()) {
        return it->second.volume;
    }
    return 1.0f;
}

void lyra::mockaudio::set_bus_muted(AudioBusHandle bus, bool muted)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.buses.find(bus.value);
    if (it != g_state.buses.end()) {
        it->second.muted = muted;
    }
}

bool lyra::mockaudio::is_bus_muted(AudioBusHandle bus)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.buses.find(bus.value);
    if (it != g_state.buses.end()) {
        return it->second.muted;
    }
    return false;
}

bool lyra::mockaudio::create_sound(const SoundDescriptor& desc, SoundHandle& out_sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    out_sound = make_mock_handle<SoundHandle>();

    MockSoundData sound{};
    sound.desc      = desc;
    sound.state     = AudioPlayState::Stopped;
    sound.cursor    = 0.0f;
    sound.volume    = desc.volume;
    sound.pitch     = desc.pitch;
    sound.looping   = desc.looping;
    sound.spatial   = desc.spatialized;
    sound.position  = desc.position;
    sound.velocity  = desc.velocity;
    sound.direction = desc.direction;

    g_state.sounds[out_sound.value] = sound;
    return true;
}

void lyra::mockaudio::delete_sound(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    g_state.sounds.erase(sound.value);
}

void lyra::mockaudio::play(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.state = AudioPlayState::Playing;
    }
}

void lyra::mockaudio::pause(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.state = AudioPlayState::Paused;
    }
}

void lyra::mockaudio::stop(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.state  = AudioPlayState::Stopped;
        it->second.cursor = 0.0f;
    }
}

void lyra::mockaudio::seek(SoundHandle sound, float seconds)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.cursor = std::max(0.0f, seconds);
    }
}

float lyra::mockaudio::get_cursor(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        return it->second.cursor;
    }
    return 0.0f;
}

AudioPlayState lyra::mockaudio::get_play_state(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        return it->second.state;
    }
    return AudioPlayState::Stopped;
}

void lyra::mockaudio::set_volume(SoundHandle sound, float volume)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.volume = glm::clamp(volume, 0.0f, 2.0f);
    }
}

float lyra::mockaudio::get_volume(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        return it->second.volume;
    }
    return 1.0f;
}

void lyra::mockaudio::set_pitch(SoundHandle sound, float pitch)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.pitch = glm::clamp(pitch, 0.1f, 4.0f);
    }
}

float lyra::mockaudio::get_pitch(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        return it->second.pitch;
    }
    return 1.0f;
}

void lyra::mockaudio::set_looping(SoundHandle sound, bool looping)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.looping = looping;
    }
}

bool lyra::mockaudio::is_looping(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        return it->second.looping;
    }
    return false;
}

void lyra::mockaudio::set_spatialization_enabled(SoundHandle sound, bool enabled)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.spatial = enabled;
    }
}

void lyra::mockaudio::set_position(SoundHandle sound, const Vector3& position)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.position = position;
    }
}

void lyra::mockaudio::set_velocity(SoundHandle sound, const Vector3& velocity)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.velocity = velocity;
    }
}

void lyra::mockaudio::set_direction(SoundHandle sound, const Vector3& direction)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.direction = direction;
    }
}

void lyra::mockaudio::set_attenuation(SoundHandle sound, AttenuationModel model, float min_dist, float max_dist, float rolloff)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.desc.attenuation_model = model;
        it->second.desc.min_distance      = min_dist;
        it->second.desc.max_distance      = max_dist;
        it->second.desc.rolloff           = rolloff;
    }
}

void lyra::mockaudio::set_cone(SoundHandle sound, float inner_angle, float outer_angle, float outer_gain)
{
    LockGuard<Mutex> lock(g_state_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it != g_state.sounds.end()) {
        it->second.desc.inner_cone_angle = inner_angle;
        it->second.desc.outer_cone_angle = outer_angle;
        it->second.desc.outer_cone_gain  = outer_gain;
    }
}

void lyra::mockaudio::set_listener_state(uint index, const AudioListenerDescriptor& desc)
{
    LockGuard<Mutex> lock(g_state_mutex);
    if (index < 4) {
        g_state.listeners[index] = desc;
    }
}

void lyra::mockaudio::play_one_shot(AudioClipHandle clip, AudioBusHandle bus, float volume, float pitch, const Vector3& position)
{
    SoundDescriptor desc{};
    desc.clip        = clip;
    desc.bus         = bus;
    desc.volume      = volume;
    desc.pitch       = pitch;
    desc.position    = position;
    desc.looping     = false;
    desc.spatialized = true;

    SoundHandle sound{};
    if (create_sound(desc, sound)) {
        play(sound);
    }
}

namespace lyra::mockaudio
{
    void prepare() {}
    void cleanup() {}

    auto create() -> AudioAPI
    {
        AudioAPI api{};
        api.get_api_name               = get_api_name;
        api.create_device              = create_device;
        api.delete_device              = delete_device;
        api.update                     = update;
        api.set_master_volume          = set_master_volume;
        api.get_master_volume          = get_master_volume;
        api.create_clip                = create_clip;
        api.delete_clip                = delete_clip;
        api.get_clip_duration          = get_clip_duration;
        api.create_bus                 = create_bus;
        api.delete_bus                 = delete_bus;
        api.set_bus_volume             = set_bus_volume;
        api.get_bus_volume             = get_bus_volume;
        api.set_bus_muted              = set_bus_muted;
        api.is_bus_muted               = is_bus_muted;
        api.create_sound               = create_sound;
        api.delete_sound               = delete_sound;
        api.play                       = play;
        api.pause                      = pause;
        api.stop                       = stop;
        api.seek                       = seek;
        api.get_cursor                 = get_cursor;
        api.get_play_state             = get_play_state;
        api.set_volume                 = set_volume;
        api.get_volume                 = get_volume;
        api.set_pitch                  = set_pitch;
        api.get_pitch                  = get_pitch;
        api.set_looping                = set_looping;
        api.is_looping                 = is_looping;
        api.set_spatialization_enabled = set_spatialization_enabled;
        api.set_position               = set_position;
        api.set_velocity               = set_velocity;
        api.set_direction              = set_direction;
        api.set_attenuation            = set_attenuation;
        api.set_cone                   = set_cone;
        api.set_listener_state         = set_listener_state;
        api.play_one_shot              = play_one_shot;
        return api;
    }
} // namespace lyra::mockaudio
