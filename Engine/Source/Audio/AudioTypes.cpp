#include <Lyra/Audio/AudioAPI.h>
#include <Lyra/Audio/AudioDescs.h>
#include <Lyra/Audio/AudioEnums.h>
#include <Lyra/Audio/AudioTypes.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Sync.h>

using namespace lyra;

// forward declarations for inlined plugins
FORWARD_DECLARE_API(lyra::mockaudio, AudioAPI)
FORWARD_DECLARE_API(lyra::miniaudio, AudioAPI)

using AudioPlugin        = Plugin<AudioAPI>;
using BuiltinAudioPlugin = BuiltinPlugin<AudioAPI>;

namespace
{
    auto get_logger() -> Logger
    {
        static Logger LOGGER = create_logger("Audio", LogLevel::info);
        return LOGGER;
    }

    struct AudioSlot
    {
        Own<AudioPlugin>        plugin;
        Own<BuiltinAudioPlugin> builtin_plugin;
        AudioAPI*               api          = nullptr;
        Optional<AudioBackend>  backend_type = std::nullopt;
        bool                    active       = false;
    };

    auto& get_slot_mutex()
    {
        static Mutex s_mtx;
        return s_mtx;
    }

    AudioSlot    g_slot;
    AudioBackend g_active_backend = AudioBackend::Mock;
} // namespace

#pragma region Sound Methods

void Sound::play()
{
    if (auto api = Audio::api(); api && api->play && handle.valid()) {
        api->play(handle);
    }
}

void Sound::pause()
{
    if (auto api = Audio::api(); api && api->pause && handle.valid()) {
        api->pause(handle);
    }
}

void Sound::stop()
{
    if (auto api = Audio::api(); api && api->stop && handle.valid()) {
        api->stop(handle);
    }
}

void Sound::seek(float seconds)
{
    if (auto api = Audio::api(); api && api->seek && handle.valid()) {
        api->seek(handle, seconds);
    }
}

float Sound::get_cursor() const
{
    if (auto api = Audio::api(); api && api->get_cursor && handle.valid()) {
        return api->get_cursor(handle);
    }
    return 0.0f;
}

AudioPlayState Sound::get_state() const
{
    if (auto api = Audio::api(); api && api->get_play_state && handle.valid()) {
        return api->get_play_state(handle);
    }
    return AudioPlayState::Stopped;
}

void Sound::set_volume(float volume)
{
    if (auto api = Audio::api(); api && api->set_volume && handle.valid()) {
        api->set_volume(handle, volume);
    }
}

float Sound::get_volume() const
{
    if (auto api = Audio::api(); api && api->get_volume && handle.valid()) {
        return api->get_volume(handle);
    }
    return 1.0f;
}

void Sound::set_pitch(float pitch)
{
    if (auto api = Audio::api(); api && api->set_pitch && handle.valid()) {
        api->set_pitch(handle, pitch);
    }
}

float Sound::get_pitch() const
{
    if (auto api = Audio::api(); api && api->get_pitch && handle.valid()) {
        return api->get_pitch(handle);
    }
    return 1.0f;
}

void Sound::set_looping(bool looping)
{
    if (auto api = Audio::api(); api && api->set_looping && handle.valid()) {
        api->set_looping(handle, looping);
    }
}

bool Sound::is_looping() const
{
    if (auto api = Audio::api(); api && api->is_looping && handle.valid()) {
        return api->is_looping(handle);
    }
    return false;
}

void Sound::set_position(const Vector3& pos)
{
    if (auto api = Audio::api(); api && api->set_position && handle.valid()) {
        api->set_position(handle, pos);
    }
}

void Sound::set_velocity(const Vector3& vel)
{
    if (auto api = Audio::api(); api && api->set_velocity && handle.valid()) {
        api->set_velocity(handle, vel);
    }
}

void Sound::set_direction(const Vector3& dir)
{
    if (auto api = Audio::api(); api && api->set_direction && handle.valid()) {
        api->set_direction(handle, dir);
    }
}

void Sound::destroy()
{
    if (auto api = Audio::api(); api && api->delete_sound && handle.valid()) {
        api->delete_sound(handle);
        handle.reset();
    }
}

#pragma endregion

#pragma region AudioClip Methods

float AudioClip::get_duration() const
{
    if (auto api = Audio::api(); api && api->get_clip_duration && handle.valid()) {
        return api->get_clip_duration(handle);
    }
    return 0.0f;
}

void AudioClip::destroy()
{
    if (auto api = Audio::api(); api && api->delete_clip && handle.valid()) {
        api->delete_clip(handle);
        handle.reset();
    }
}

#pragma endregion

#pragma region AudioBus Methods

void AudioBus::set_volume(float volume)
{
    if (auto api = Audio::api(); api && api->set_bus_volume && handle.valid()) {
        api->set_bus_volume(handle, volume);
    }
}

float AudioBus::get_volume() const
{
    if (auto api = Audio::api(); api && api->get_bus_volume && handle.valid()) {
        return api->get_bus_volume(handle);
    }
    return 1.0f;
}

void AudioBus::set_muted(bool muted)
{
    if (auto api = Audio::api(); api && api->set_bus_muted && handle.valid()) {
        api->set_bus_muted(handle, muted);
    }
}

bool AudioBus::is_muted() const
{
    if (auto api = Audio::api(); api && api->is_bus_muted && handle.valid()) {
        return api->is_bus_muted(handle);
    }
    return false;
}

void AudioBus::destroy()
{
    if (auto api = Audio::api(); api && api->delete_bus && handle.valid()) {
        api->delete_bus(handle);
        handle.reset();
    }
}

#pragma endregion

#pragma region Audio Subsystem Coordinator

Result<void> Audio::init(AudioBackend backend)
{
    LockGuard<Mutex> lock(get_slot_mutex());

    if (g_slot.active && g_slot.backend_type == backend && g_slot.api) {
        return {};
    }

    g_slot.builtin_plugin.reset();
    g_slot.plugin.reset();
    g_slot.api          = nullptr;
    g_slot.active       = false;
    g_slot.backend_type = backend;

    switch (backend) {
        case AudioBackend::Miniaudio:
        {
            g_slot.builtin_plugin = make_own<BuiltinAudioPlugin>(
                lyra::miniaudio::create,
                lyra::miniaudio::prepare,
                lyra::miniaudio::cleanup);
            g_slot.api = g_slot.builtin_plugin->get_api();
            break;
        }
        case AudioBackend::Mock:
        {
            g_slot.builtin_plugin = make_own<BuiltinAudioPlugin>(
                lyra::mockaudio::create,
                lyra::mockaudio::prepare,
                lyra::mockaudio::cleanup);
            g_slot.api = g_slot.builtin_plugin->get_api();
            if (!g_slot.api) {
                g_slot.plugin = make_own<AudioPlugin>("lyra-mockaudio");
                g_slot.api    = g_slot.plugin->get_api();
            }
            break;
        }
        case AudioBackend::Custom:
        {
            get_logger()->error("Use Audio::init(StringView custom_backend) for custom plugins");
            return make_unexpected("Use Audio::init(StringView custom_backend) for custom plugins");
        }
    }

    if (!g_slot.api) {
        get_logger()->error("Failed to initialize audio backend");
        return make_unexpected("Failed to initialize audio backend");
    }

    // create primary output device
    AudioDeviceDescriptor device_desc{};
    if (g_slot.api->create_device && !g_slot.api->create_device(device_desc)) {
        get_logger()->error("Audio device creation failed on active backend");
        return make_unexpected("Audio device creation failed on active backend");
    }

    g_slot.active    = true;
    g_active_backend = backend;
    get_logger()->info("Audio subsystem initialized with backend: {}", g_slot.api->get_api_name ? g_slot.api->get_api_name() : "unknown");
    return {};
}

Result<void> Audio::init(StringView custom_backend)
{
    LockGuard<Mutex> lock(get_slot_mutex());

    g_slot.builtin_plugin.reset();
    g_slot.plugin.reset();
    g_slot.api          = nullptr;
    g_slot.active       = false;
    g_slot.backend_type = AudioBackend::Custom;

    g_slot.plugin = make_own<AudioPlugin>();
    String plugin_name(custom_backend);
    auto loaded = g_slot.plugin->load(plugin_name.c_str());
    if (!loaded) {
        get_logger()->error("Failed to load custom audio plugin: {}: {}", custom_backend, loaded.error());
        return make_unexpected("Failed to load custom audio plugin '" + plugin_name + "': " + loaded.error());
    }
    g_slot.api = g_slot.plugin->get_api();

    if (!g_slot.api) {
        get_logger()->error("Failed to get API from custom audio plugin: {}", custom_backend);
        return make_unexpected("Failed to get API from custom audio plugin: " + plugin_name);
    }

    AudioDeviceDescriptor device_desc{};
    if (g_slot.api->create_device && !g_slot.api->create_device(device_desc)) {
        get_logger()->error("Audio device creation failed on custom backend");
        return make_unexpected("Audio device creation failed on custom backend");
    }

    g_slot.active    = true;
    g_active_backend = AudioBackend::Custom;
    get_logger()->info("Audio subsystem initialized with custom backend: {}", custom_backend);
    return {};
}

void Audio::shutdown()
{
    LockGuard<Mutex> lock(get_slot_mutex());

    if (!g_slot.active) {
        return;
    }

    if (g_slot.api && g_slot.api->delete_device) {
        g_slot.api->delete_device();
    }

    g_slot.builtin_plugin.reset();
    g_slot.plugin.reset();
    g_slot.api    = nullptr;
    g_slot.active = false;
    get_logger()->info("Audio subsystem shut down successfully");
}

AudioAPI* Audio::api()
{
    return g_slot.api;
}

AudioBackend Audio::get_active_backend()
{
    return g_active_backend;
}

void Audio::update(float delta_time)
{
    if (g_slot.api && g_slot.api->update) {
        g_slot.api->update(delta_time);
    }
}

void Audio::set_master_volume(float volume)
{
    if (g_slot.api && g_slot.api->set_master_volume) {
        g_slot.api->set_master_volume(volume);
    }
}

float Audio::get_master_volume()
{
    if (g_slot.api && g_slot.api->get_master_volume) {
        return g_slot.api->get_master_volume();
    }
    return 1.0f;
}

AudioClip Audio::create_clip(const AudioClipDescriptor& desc)
{
    AudioClip clip{};
    if (g_slot.api && g_slot.api->create_clip) {
        g_slot.api->create_clip(desc, clip.handle);
    }
    return clip;
}

Sound Audio::create_sound(const SoundDescriptor& desc)
{
    Sound sound{};
    if (g_slot.api && g_slot.api->create_sound) {
        g_slot.api->create_sound(desc, sound.handle);
    }
    return sound;
}

AudioBus Audio::create_bus(const AudioBusDescriptor& desc)
{
    AudioBus bus{};
    if (g_slot.api && g_slot.api->create_bus) {
        g_slot.api->create_bus(desc, bus.handle);
    }
    return bus;
}

void Audio::set_listener(uint index, const AudioListenerDescriptor& desc)
{
    if (g_slot.api && g_slot.api->set_listener_state) {
        g_slot.api->set_listener_state(index, desc);
    }
}

void Audio::play_one_shot(AudioClipHandle clip, const Vector3& pos, float volume)
{
    if (g_slot.api && g_slot.api->play_one_shot && clip.valid()) {
        g_slot.api->play_one_shot(clip, AudioBusHandle{}, volume, 1.0f, pos);
    }
}

#pragma endregion
