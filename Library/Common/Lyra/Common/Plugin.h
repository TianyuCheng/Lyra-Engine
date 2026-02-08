#pragma once

#ifndef LYRA_LIBRARY_COMMON_PLUGIN_H
#define LYRA_LIBRARY_COMMON_PLUGIN_H

// system headers
#include <sstream>

// library headers
#include <Lyra/Common/String.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Msgbox.h>

// Windows specific
#ifdef _WIN32
#undef APIENTRY // undefine APIENTRY macro
#include <windows.h>
#undef CreateWindow // undefine CreateWindow macro
#define LYRA_EXPORT extern "C" __declspec(dllexport)
#define LYRA_PLUGIN HMODULE
#endif

// Unix-based OS
#ifndef _WIN32
#include <dlfcn.h>
#define LYRA_EXPORT extern "C" __attribute__((visibility("default")))
#define LYRA_PLUGIN void*
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

        explicit Plugin() = delete;
        explicit Plugin(const char* name) { load(name); }
        explicit Plugin(const Plugin& other) = delete;
        explicit Plugin(Plugin&& other)
        {
            api     = other.api;
            name    = other.name;
            plugin  = other.plugin;
            create  = other.create;
            prepare = other.prepare;
            cleanup = other.cleanup;

            other.api     = nullptr;
            other.plugin  = nullptr;
            other.create  = nullptr;
            other.prepare = nullptr;
            other.cleanup = nullptr;
        }
        virtual ~Plugin() { unload(); }

        // load plugin and function
        void load(const char* name)
        {
            this->name = name;

            unload();

            load_dll();
            if (!plugin) {
                std::stringstream ss;
                ss << "Load Plugin: " << CString(name);
                show_error(ss.str(), get_error());
                exit(1);
            }

            load_api();
            if (!create) {
                std::stringstream ss;
                ss << "Load API: " << CString(name) << "::Create";
                show_error(ss.str(), get_error());
                exit(1);
            }

            if (prepare) {
                prepare();
            }

            api = create();
        }

        // unload plugin if necessary
        void unload()
        {
            if (cleanup) {
                cleanup();
            }

            if (plugin) {
                unload_dll();
                plugin = nullptr;
            }
        }

        APIType* get_api() { return &api; }

        APIType* get_api() const { return &api; }

    private:
        void load_dll();
        void unload_dll();
        void load_api();
        auto get_error() -> String;

    private:
        APIType     api;
        CString     name    = nullptr;
        LYRA_PLUGIN plugin  = nullptr;
        CreateFn    create  = nullptr;
        PrepareFn   prepare = nullptr;
        PrepareFn   cleanup = nullptr;
    };

} // end of namespace lyra

#ifdef _WIN32
template <typename APIType>
void lyra::Plugin<APIType>::load_dll()
{
    plugin = LoadLibrary(name);
}

template <typename APIType>
void lyra::Plugin<APIType>::unload_dll()
{
    FreeLibrary(plugin);
}

template <typename APIType>
void lyra::Plugin<APIType>::load_api()
{
    create  = (CreateFn)GetProcAddress(plugin, "create");
    prepare = (PrepareFn)GetProcAddress(plugin, "prepare");
    cleanup = (CleanupFn)GetProcAddress(plugin, "cleanup");
}

template <typename APIType>
auto lyra::Plugin<APIType>::get_error() -> String
{
    // Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return CString(); // No error message has been recorded
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
void lyra::Plugin<APIType>::load_dll()
{
    std::stringstream ss;
    ss << name << ".so";
    String path = ss.str();

    plugin = dlopen(path.c_str(), RTLD_LAZY);
}

template <typename APIType>
void lyra::Plugin<APIType>::unload_dll()
{
    dlclose(plugin);
}

template <typename APIType>
void lyra::Plugin<APIType>::load_api()
{
    create  = (CreateFn)dlsym(plugin, "create");
    prepare = (PrepareFn)dlsym(plugin, "prepare");
    cleanup = (CleanupFn)dlsym(plugin, "cleanup");
}

template <typename APIType>
auto lyra::Plugin<APIType>::get_error() -> String
{
    return CString(dlerror());
}
#endif

#endif // LYRA_LIBRARY_COMMON_PLUGIN_H
