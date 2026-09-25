#pragma once

#ifndef LYRA_ENGINE_UTILITIES_PLUGIN_H
#define LYRA_ENGINE_UTILITIES_PLUGIN_H

// system headers
#include <sstream>

// library headers
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Msgbox.h>

// Windows specific
#ifdef _WIN32
#undef APIENTRY // undefine APIENTRY macro
#include <Lyra/Utilities/Compatibility.h>
#define LYRA_PLUGIN HMODULE
#endif

// Unix-based OS
#ifndef _WIN32
#include <dlfcn.h>
#define LYRA_PLUGIN void*
#endif

// extern C
#ifdef __cplusplus
#define LYRA_EXTERN_C extern "C"
#else
#define LYRA_EXTERN_C
#endif

// symbol export / import visibility
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LYRA_BUILD_SHARED)
#define LYRA_API __declspec(dllexport)
#else
#define LYRA_API __declspec(dllimport)
#endif
#define LYRA_EXPORT LYRA_EXTERN_C __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define LYRA_API    __attribute__((visibility("default")))
#define LYRA_EXPORT LYRA_EXTERN_C __attribute__((visibility("default")))
#else
#define LYRA_API
#define LYRA_EXPORT LYRA_EXTERN_C
#endif

// declaration within namespace
namespace lyra
{
    template <typename APIType>
    class Plugin
    {
    public:
        using CreateFn  = APIType (*)();
        using PrepareFn = void (*)();
        using CleanupFn = void (*)();

        Plugin() = default;
        explicit Plugin(const char* name) { load(name); }
        Plugin(const Plugin& other) = delete;
        Plugin(Plugin&& other) noexcept
        {
            api     = other.api;
            name    = other.name;
            plugin  = other.plugin;
            create  = other.create;
            prepare = other.prepare;
            cleanup = other.cleanup;

            other.api     = {};
            other.name    = nullptr;
            other.plugin  = nullptr;
            other.create  = nullptr;
            other.prepare = nullptr;
            other.cleanup = nullptr;
        }
        Plugin& operator=(const Plugin& other) = delete;
        Plugin& operator=(Plugin&& other) noexcept
        {
            if (this != &other) {
                unload();

                api     = other.api;
                name    = other.name;
                plugin  = other.plugin;
                create  = other.create;
                prepare = other.prepare;
                cleanup = other.cleanup;

                other.api     = {};
                other.name    = nullptr;
                other.plugin  = nullptr;
                other.create  = nullptr;
                other.prepare = nullptr;
                other.cleanup = nullptr;
            }
            return *this;
        }
        virtual ~Plugin() { unload(); }

        // load plugin and function
        bool load(const char* name)
        {
            auto new_plugin = load_dll(name);
            if (!new_plugin) {
                String err = get_error();
                spdlog::error("Load Plugin: {} failed: {}", name, err);
                return false;
            }

            CreateFn  new_create  = nullptr;
            PrepareFn new_prepare = nullptr;
            CleanupFn new_cleanup = nullptr;
            if (!load_api(new_plugin, new_create, new_prepare, new_cleanup)) {
                String err = get_error();
                spdlog::error("Load API: {}::Create failed: {}", name, err);
                unload_dll(new_plugin);
                return false;
            }

            unload();

            this->name    = name;
            this->plugin  = new_plugin;
            this->create  = new_create;
            this->prepare = new_prepare;
            this->cleanup = new_cleanup;

            if (prepare) {
                prepare();
            }

            api = create();
            return true;
        }

        // unload plugin if necessary
        void unload()
        {
            if (cleanup) {
                cleanup();
            }

            if (plugin) {
                unload_dll(plugin);
                plugin = nullptr;
            }

            create  = nullptr;
            prepare = nullptr;
            cleanup = nullptr;
            name    = nullptr;
            api     = {};
        }

        bool is_loaded() const { return plugin != nullptr; }

        explicit operator bool() const { return is_loaded(); }

        APIType* get_api() { return &api; }

        APIType* get_api() const { return &api; }

        static auto get_error() -> String;

    private:
        static auto load_dll(const char* name) -> LYRA_PLUGIN;
        static void unload_dll(LYRA_PLUGIN lib);
        static bool load_api(LYRA_PLUGIN lib, CreateFn& out_create, PrepareFn& out_prepare, CleanupFn& out_cleanup);

    private:
        APIType     api{};
        CString     name    = nullptr;
        LYRA_PLUGIN plugin  = nullptr;
        CreateFn    create  = nullptr;
        PrepareFn   prepare = nullptr;
        CleanupFn   cleanup = nullptr;
    };

    template <typename APIType>
    class BuiltinPlugin
    {
    public:
        using CreateFn  = APIType (*)();
        using PrepareFn = void (*)();
        using CleanupFn = void (*)();

        explicit BuiltinPlugin() = delete;
        explicit BuiltinPlugin(CreateFn create, PrepareFn prepare = nullptr, CleanupFn cleanup = nullptr)
            : create(create), prepare(prepare), cleanup(cleanup)
        {
            if (prepare) prepare();
            api = create();
        }
        explicit BuiltinPlugin(const BuiltinPlugin& other) = delete;
        explicit BuiltinPlugin(BuiltinPlugin&& other)
        {
            api     = other.api;
            create  = other.create;
            prepare = other.prepare;
            cleanup = other.cleanup;

            other.create  = nullptr;
            other.prepare = nullptr;
            other.cleanup = nullptr;
        }
        virtual ~BuiltinPlugin()
        {
            if (cleanup) cleanup();
        }

        APIType* get_api() { return &api; }

        APIType* get_api() const { return &api; }

    private:
        APIType   api;
        CreateFn  create  = nullptr;
        PrepareFn prepare = nullptr;
        CleanupFn cleanup = nullptr;
    };

} // end of namespace lyra

#ifdef _WIN32
template <typename APIType>
auto lyra::Plugin<APIType>::load_dll(const char* name) -> LYRA_PLUGIN
{
    return LoadLibraryA(name);
}

template <typename APIType>
void lyra::Plugin<APIType>::unload_dll(LYRA_PLUGIN lib)
{
    if (lib) {
        FreeLibrary(lib);
    }
}

template <typename APIType>
bool lyra::Plugin<APIType>::load_api(LYRA_PLUGIN lib, CreateFn& out_create, PrepareFn& out_prepare, CleanupFn& out_cleanup)
{
    out_create = (CreateFn)GetProcAddress(lib, "create");
    if (!out_create) return false;
    out_prepare = (PrepareFn)GetProcAddress(lib, "prepare");
    out_cleanup = (CleanupFn)GetProcAddress(lib, "cleanup");
    return true;
}

template <typename APIType>
auto lyra::Plugin<APIType>::get_error() -> String
{
    // Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return ""; // No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    // Copy the error message into a std::string.
    String message(messageBuffer, size);

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);
    return message;
}
#else
template <typename APIType>
auto lyra::Plugin<APIType>::load_dll(const char* name) -> LYRA_PLUGIN
{
    std::stringstream ss;
#if defined(__APPLE__)
    ss << name << ".dylib";
#else
    ss << name << ".so";
#endif
    String path = ss.str();

    return dlopen(path.c_str(), RTLD_LAZY);
}

template <typename APIType>
void lyra::Plugin<APIType>::unload_dll(LYRA_PLUGIN lib)
{
    if (lib) {
        dlclose(lib);
    }
}

template <typename APIType>
bool lyra::Plugin<APIType>::load_api(LYRA_PLUGIN lib, CreateFn& out_create, PrepareFn& out_prepare, CleanupFn& out_cleanup)
{
    out_create = (CreateFn)dlsym(lib, "create");
    if (!out_create) return false;
    out_prepare = (PrepareFn)dlsym(lib, "prepare");
    out_cleanup = (CleanupFn)dlsym(lib, "cleanup");
    return true;
}

template <typename APIType>
auto lyra::Plugin<APIType>::get_error() -> String
{
    const char* err = dlerror();
    return err ? String(err) : String();
}
#endif

#endif // LYRA_ENGINE_UTILITIES_PLUGIN_H
