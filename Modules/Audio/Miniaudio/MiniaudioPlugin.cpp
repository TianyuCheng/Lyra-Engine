#include "MiniaudioPlugin.h"

#include <Lyra/Audio/AudioDescs.h>
#include <Lyra/Audio/AudioEnums.h>
#include <Lyra/Audio/AudioTypes.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Sync.h>

// miniaudio implementation (single translation unit)
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

using namespace lyra;

static Atomic<uint> g_handle_counter{1};

template <typename T>
static auto make_handle() -> T
{
    uint id = g_handle_counter.fetch_add(1);
    return T(id);
}

static auto get_logger() -> Logger
{
    static Logger LOGGER = create_logger("Miniaudio", LogLevel::info);
    return LOGGER;
}

struct MiniClip
{
    float  sample_rate = 0.0f;
    ulong  frames      = 0;
    uint   channels    = 0;
    void*  pcm         = nullptr; ///< decoded interleaved float32 (nullptr when streamed)
    String stream_path;           ///< native path when streamed from disk
    bool   streamed = false;
};

struct MiniBus
{
    ma_sound_group group{};
    float          volume = 1.0f;
    bool           muted  = false;
};

struct MiniSound
{
    ma_sound        sound{};
    ma_audio_buffer buffer{};
    bool            has_buffer = false;
    AudioPlayState  state      = AudioPlayState::Stopped;
    float           volume     = 1.0f;
    float           pitch      = 1.0f;
    bool            one_shot   = false;
};

struct MiniState
{
    ma_engine                     engine{};
    bool                          active = false;
    HashMap<uint, MiniClip>       clips;
    HashMap<uint, Own<MiniBus>>   buses;
    HashMap<uint, Own<MiniSound>> sounds;
};

static Mutex     g_mutex;
static MiniState g_state;

static auto h_to_ma_model(AttenuationModel model) -> ma_attenuation_model
{
    switch (model) {
        case AttenuationModel::Inverse:
            return ma_attenuation_model_inverse;
        case AttenuationModel::Linear:
            return ma_attenuation_model_linear;
        case AttenuationModel::Exponential:
            return ma_attenuation_model_exponential;
        case AttenuationModel::None:
            return ma_attenuation_model_none;
    }
    return ma_attenuation_model_inverse;
}

static auto h_find_sound(SoundHandle handle) -> MiniSound*
{
    auto it = g_state.sounds.find(handle.value);
    return it != g_state.sounds.end() ? it->second.get() : nullptr;
}

static auto h_find_bus(AudioBusHandle handle) -> MiniBus*
{
    auto it = g_state.buses.find(handle.value);
    return it != g_state.buses.end() ? it->second.get() : nullptr;
}

static void h_destroy_sound(MiniSound& s)
{
    ma_sound_uninit(&s.sound);
    if (s.has_buffer) ma_audio_buffer_uninit(&s.buffer);
}

static auto h_effective_volume(const MiniBus& bus) -> float
{
    return bus.muted ? 0.0f : bus.volume;
}

CString lyra::miniaudio::get_api_name()
{
    return "Miniaudio";
}

bool lyra::miniaudio::create_device(const AudioDeviceDescriptor& desc)
{
    LockGuard<Mutex> lock(g_mutex);
    if (g_state.active) return true;

    ma_engine_config config   = ma_engine_config_init();
    config.channels           = desc.channels;
    config.sampleRate         = desc.sample_rate;
    config.periodSizeInFrames = desc.buffer_frame_size;
    config.listenerCount      = 4;

    if (ma_engine_init(&config, &g_state.engine) != MA_SUCCESS) {
        get_logger()->error("ma_engine_init failed (no usable audio device?)");
        return false;
    }

    ma_engine_set_volume(&g_state.engine, desc.master_volume);
    g_state.active = true;
    return true;
}

void lyra::miniaudio::delete_device()
{
    LockGuard<Mutex> lock(g_mutex);
    if (!g_state.active) return;

    for (auto& [id, sound] : g_state.sounds)
        h_destroy_sound(*sound);
    g_state.sounds.clear();

    // handles grow monotonically, so uninit children (newer) before parents
    Vector<uint> bus_ids;
    for (auto& [id, bus] : g_state.buses)
        bus_ids.push_back(id);
    std::sort(bus_ids.begin(), bus_ids.end(), std::greater<uint>());
    for (uint id : bus_ids)
        ma_sound_group_uninit(&g_state.buses[id]->group);
    g_state.buses.clear();

    for (auto& [id, clip] : g_state.clips) {
        if (clip.pcm) ma_free(clip.pcm, nullptr);
    }
    g_state.clips.clear();

    ma_engine_uninit(&g_state.engine);
    g_state.active = false;
}

void lyra::miniaudio::update(float /*delta_time*/)
{
    LockGuard<Mutex> lock(g_mutex);
    if (!g_state.active) return;

    // reap finished one-shots; refresh state of finished sounds
    Vector<uint> finished;
    for (auto& [id, s] : g_state.sounds) {
        if (s->state == AudioPlayState::Playing && ma_sound_at_end(&s->sound)) {
            s->state = AudioPlayState::Stopped;
            if (s->one_shot) finished.push_back(id);
        }
    }
    for (uint id : finished) {
        h_destroy_sound(*g_state.sounds[id]);
        g_state.sounds.erase(id);
    }
}

void lyra::miniaudio::set_master_volume(float volume)
{
    LockGuard<Mutex> lock(g_mutex);
    if (g_state.active) ma_engine_set_volume(&g_state.engine, glm::clamp(volume, 0.0f, 2.0f));
}

float lyra::miniaudio::get_master_volume()
{
    LockGuard<Mutex> lock(g_mutex);
    return g_state.active ? ma_engine_get_volume(&g_state.engine) : 1.0f;
}

bool lyra::miniaudio::create_clip(const AudioClipDescriptor& desc, AudioClipHandle& out_clip)
{
    LockGuard<Mutex> lock(g_mutex);
    if (!g_state.active) return false;

    MiniClip clip{};
    if (desc.stream_from_disk) {
        if (desc.stream_path.empty()) return false;
        ma_decoder decoder;
        String     path(desc.stream_path);
        if (ma_decoder_init_file(path.c_str(), nullptr, &decoder) != MA_SUCCESS) return false;
        ma_uint64 frames = 0;
        ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
        clip.sample_rate = static_cast<float>(decoder.outputSampleRate);
        clip.channels    = decoder.outputChannels;
        clip.frames      = frames;
        ma_decoder_uninit(&decoder);
        clip.streamed    = true;
        clip.stream_path = std::move(path);
    } else {
        if (desc.data.size() == 0) return false;
        uint              rate   = ma_engine_get_sample_rate(&g_state.engine);
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, rate);
        ma_uint64         frames = 0;
        void*             pcm    = nullptr;
        if (ma_decode_memory(desc.data.data(), desc.data.size(), &config, &frames, &pcm) != MA_SUCCESS) {
            get_logger()->error("failed to decode audio clip (format '{}')", desc.source_format);
            return false;
        }
        clip.sample_rate = static_cast<float>(rate);
        clip.channels    = config.channels;
        clip.frames      = frames;
        clip.pcm         = pcm;
    }

    out_clip                      = make_handle<AudioClipHandle>();
    g_state.clips[out_clip.value] = std::move(clip);
    return true;
}

void lyra::miniaudio::delete_clip(AudioClipHandle clip)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             it = g_state.clips.find(clip.value);
    if (it == g_state.clips.end()) return;
    if (it->second.pcm) ma_free(it->second.pcm, nullptr);
    g_state.clips.erase(it);
}

float lyra::miniaudio::get_clip_duration(AudioClipHandle clip)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             it = g_state.clips.find(clip.value);
    if (it == g_state.clips.end() || it->second.sample_rate <= 0.0f) return 0.0f;
    return static_cast<float>(it->second.frames) / it->second.sample_rate;
}

bool lyra::miniaudio::create_bus(const AudioBusDescriptor& desc, AudioBusHandle& out_bus)
{
    LockGuard<Mutex> lock(g_mutex);
    if (!g_state.active) return false;

    auto parent = h_find_bus(desc.parent);
    auto bus    = make_own<MiniBus>();
    if (ma_sound_group_init(&g_state.engine, 0, parent ? &parent->group : nullptr, &bus->group) != MA_SUCCESS) {
        return false;
    }
    bus->volume = desc.volume;
    bus->muted  = desc.muted;
    ma_sound_group_set_volume(&bus->group, h_effective_volume(*bus));

    out_bus                      = make_handle<AudioBusHandle>();
    g_state.buses[out_bus.value] = std::move(bus);
    return true;
}

void lyra::miniaudio::delete_bus(AudioBusHandle bus)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             it = g_state.buses.find(bus.value);
    if (it == g_state.buses.end()) return;
    ma_sound_group_uninit(&it->second->group);
    g_state.buses.erase(it);
}

void lyra::miniaudio::set_bus_volume(AudioBusHandle bus, float volume)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto b = h_find_bus(bus)) {
        b->volume = glm::clamp(volume, 0.0f, 2.0f);
        ma_sound_group_set_volume(&b->group, h_effective_volume(*b));
    }
}

float lyra::miniaudio::get_bus_volume(AudioBusHandle bus)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             b = h_find_bus(bus);
    return b ? b->volume : 1.0f;
}

void lyra::miniaudio::set_bus_muted(AudioBusHandle bus, bool muted)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto b = h_find_bus(bus)) {
        b->muted = muted;
        ma_sound_group_set_volume(&b->group, h_effective_volume(*b));
    }
}

bool lyra::miniaudio::is_bus_muted(AudioBusHandle bus)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             b = h_find_bus(bus);
    return b ? b->muted : false;
}

bool lyra::miniaudio::create_sound(const SoundDescriptor& desc, SoundHandle& out_sound)
{
    LockGuard<Mutex> lock(g_mutex);
    if (!g_state.active) return false;

    auto clip_it = g_state.clips.find(desc.clip.value);
    if (clip_it == g_state.clips.end()) return false;
    const MiniClip& clip = clip_it->second;

    auto bus   = h_find_bus(desc.bus);
    auto group = bus ? &bus->group : nullptr;
    auto sound = make_own<MiniSound>();

    ma_uint32 flags = desc.spatialized ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (clip.streamed) {
        flags |= MA_SOUND_FLAG_STREAM;
        if (ma_sound_init_from_file(&g_state.engine, clip.stream_path.c_str(), flags, group, nullptr, &sound->sound) != MA_SUCCESS) {
            return false;
        }
    } else {
        ma_audio_buffer_config config = ma_audio_buffer_config_init(ma_format_f32, clip.channels, clip.frames, clip.pcm, nullptr);
        if (ma_audio_buffer_init(&config, &sound->buffer) != MA_SUCCESS) return false;
        sound->has_buffer = true;
        if (ma_sound_init_from_data_source(&g_state.engine, &sound->buffer, flags, group, &sound->sound) != MA_SUCCESS) {
            ma_audio_buffer_uninit(&sound->buffer);
            return false;
        }
    }

    sound->volume = desc.volume;
    sound->pitch  = desc.pitch;
    ma_sound_set_volume(&sound->sound, desc.volume);
    ma_sound_set_pitch(&sound->sound, desc.pitch);
    ma_sound_set_looping(&sound->sound, desc.looping);
    ma_sound_set_attenuation_model(&sound->sound, h_to_ma_model(desc.attenuation_model));
    ma_sound_set_min_distance(&sound->sound, desc.min_distance);
    ma_sound_set_max_distance(&sound->sound, desc.max_distance);
    ma_sound_set_rolloff(&sound->sound, desc.rolloff);
    ma_sound_set_cone(&sound->sound, desc.inner_cone_angle, desc.outer_cone_angle, desc.outer_cone_gain);
    ma_sound_set_position(&sound->sound, desc.position.x, desc.position.y, desc.position.z);
    ma_sound_set_velocity(&sound->sound, desc.velocity.x, desc.velocity.y, desc.velocity.z);
    ma_sound_set_direction(&sound->sound, desc.direction.x, desc.direction.y, desc.direction.z);

    out_sound                       = make_handle<SoundHandle>();
    g_state.sounds[out_sound.value] = std::move(sound);
    return true;
}

void lyra::miniaudio::delete_sound(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             it = g_state.sounds.find(sound.value);
    if (it == g_state.sounds.end()) return;
    h_destroy_sound(*it->second);
    g_state.sounds.erase(it);
}

void lyra::miniaudio::play(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) {
        if (ma_sound_at_end(&s->sound)) ma_sound_seek_to_pcm_frame(&s->sound, 0);
        ma_sound_start(&s->sound);
        s->state = AudioPlayState::Playing;
    }
}

void lyra::miniaudio::pause(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) {
        ma_sound_stop(&s->sound);
        s->state = AudioPlayState::Paused;
    }
}

void lyra::miniaudio::stop(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) {
        ma_sound_stop(&s->sound);
        ma_sound_seek_to_pcm_frame(&s->sound, 0);
        s->state = AudioPlayState::Stopped;
    }
}

void lyra::miniaudio::seek(SoundHandle sound, float seconds)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) {
        auto frame = static_cast<ma_uint64>(std::max(0.0f, seconds) * ma_engine_get_sample_rate(&g_state.engine));
        ma_sound_seek_to_pcm_frame(&s->sound, frame);
    }
}

float lyra::miniaudio::get_cursor(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    float            seconds = 0.0f;
    if (auto s = h_find_sound(sound)) ma_sound_get_cursor_in_seconds(&s->sound, &seconds);
    return seconds;
}

AudioPlayState lyra::miniaudio::get_play_state(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             s = h_find_sound(sound);
    if (!s) return AudioPlayState::Stopped;
    if (s->state == AudioPlayState::Playing && ma_sound_at_end(&s->sound)) return AudioPlayState::Stopped;
    return s->state;
}

void lyra::miniaudio::set_volume(SoundHandle sound, float volume)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) {
        s->volume = glm::clamp(volume, 0.0f, 2.0f);
        ma_sound_set_volume(&s->sound, s->volume);
    }
}

float lyra::miniaudio::get_volume(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             s = h_find_sound(sound);
    return s ? s->volume : 1.0f;
}

void lyra::miniaudio::set_pitch(SoundHandle sound, float pitch)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) {
        s->pitch = glm::clamp(pitch, 0.1f, 4.0f);
        ma_sound_set_pitch(&s->sound, s->pitch);
    }
}

float lyra::miniaudio::get_pitch(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             s = h_find_sound(sound);
    return s ? s->pitch : 1.0f;
}

void lyra::miniaudio::set_looping(SoundHandle sound, bool looping)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) ma_sound_set_looping(&s->sound, looping);
}

bool lyra::miniaudio::is_looping(SoundHandle sound)
{
    LockGuard<Mutex> lock(g_mutex);
    auto             s = h_find_sound(sound);
    return s ? ma_sound_is_looping(&s->sound) : false;
}

void lyra::miniaudio::set_spatialization_enabled(SoundHandle sound, bool enabled)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) ma_sound_set_spatialization_enabled(&s->sound, enabled);
}

void lyra::miniaudio::set_position(SoundHandle sound, const Vector3& position)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) ma_sound_set_position(&s->sound, position.x, position.y, position.z);
}

void lyra::miniaudio::set_velocity(SoundHandle sound, const Vector3& velocity)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) ma_sound_set_velocity(&s->sound, velocity.x, velocity.y, velocity.z);
}

void lyra::miniaudio::set_direction(SoundHandle sound, const Vector3& direction)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) ma_sound_set_direction(&s->sound, direction.x, direction.y, direction.z);
}

void lyra::miniaudio::set_attenuation(SoundHandle sound, AttenuationModel model, float min_dist, float max_dist, float rolloff)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) {
        ma_sound_set_attenuation_model(&s->sound, h_to_ma_model(model));
        ma_sound_set_min_distance(&s->sound, min_dist);
        ma_sound_set_max_distance(&s->sound, max_dist);
        ma_sound_set_rolloff(&s->sound, rolloff);
    }
}

void lyra::miniaudio::set_cone(SoundHandle sound, float inner_angle, float outer_angle, float outer_gain)
{
    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(sound)) ma_sound_set_cone(&s->sound, inner_angle, outer_angle, outer_gain);
}

void lyra::miniaudio::set_listener_state(uint index, const AudioListenerDescriptor& desc)
{
    LockGuard<Mutex> lock(g_mutex);
    if (!g_state.active || index >= ma_engine_get_listener_count(&g_state.engine)) return;
    ma_engine_listener_set_position(&g_state.engine, index, desc.position.x, desc.position.y, desc.position.z);
    ma_engine_listener_set_velocity(&g_state.engine, index, desc.velocity.x, desc.velocity.y, desc.velocity.z);
    ma_engine_listener_set_direction(&g_state.engine, index, desc.forward.x, desc.forward.y, desc.forward.z);
    ma_engine_listener_set_world_up(&g_state.engine, index, desc.up.x, desc.up.y, desc.up.z);
}

void lyra::miniaudio::play_one_shot(AudioClipHandle clip, AudioBusHandle bus, float volume, float pitch, const Vector3& position)
{
    SoundDescriptor desc{};
    desc.clip     = clip;
    desc.bus      = bus;
    desc.volume   = volume;
    desc.pitch    = pitch;
    desc.position = position;

    SoundHandle handle{};
    if (!create_sound(desc, handle)) return;

    LockGuard<Mutex> lock(g_mutex);
    if (auto s = h_find_sound(handle)) {
        s->one_shot = true;
        s->state    = AudioPlayState::Playing;
        ma_sound_start(&s->sound);
    }
}

void lyra::miniaudio::prepare() {}
void lyra::miniaudio::cleanup() {}

auto lyra::miniaudio::create() -> AudioAPI
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
