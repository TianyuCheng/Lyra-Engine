#pragma once

#ifndef LYRA_LIBRARY_ENGINE_APPLET_APPLICATION_H
#define LYRA_LIBRARY_ENGINE_APPLET_APPLICATION_H

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Plugin/WSI/WSITypes.h>
#include <Lyra/Plugin/SLC/SLCTypes.h>
#include <Lyra/Plugin/RHI/RHITypes.h>

namespace lyra
{
    using AppWindowDescriptor = WindowDescriptor;

    enum struct AppEvent : uint
    {
        INIT,
        DESTROY,
        RESIZE,

        UPDATE,
        UPDATE_PRE,
        UPDATE_POST,
        UPDATE_FIXED,

        UI,
        UI_PRE,
        UI_POST,

        RENDER,
        RENDER_PRE,
        RENDER_POST,
    };

    struct AppGraphicsDescriptor
    {
        RHIBackend backend;
        RHIFlags   flags;
        uint       frames = 3;
    };

    struct AppCompilerDescriptor
    {
        CompileTarget target;
        CompileFlags  flags;
    };

    struct AppDescriptor
    {
        friend struct Application;

    public:
        AppDescriptor& with_title(CString title);
        AppDescriptor& with_fullscreen(bool enable = true);
        AppDescriptor& with_window_maximized();
        AppDescriptor& with_window_extent(uint width, uint height);
        AppDescriptor& with_graphics_backend(RHIBackend backend);
        AppDescriptor& with_graphics_validation(bool debug = true, bool validation = true);
        AppDescriptor& with_frames_in_flight(uint frames_in_flight);

    private:
        AppWindowDescriptor   wsi;
        AppGraphicsDescriptor rhi;
        AppCompilerDescriptor slc;
    };

    struct Backbuffer
    {
        GPUTextureHandle     texture;
        GPUTextureViewHandle texview;
        GPUTextureFormat     format;
        GPUExtent2D          extent;
    };

    struct Application
    {
        static constexpr size_t STAGE_COUNT = magic_enum::enum_count<AppEvent>();

    public:
        using Callback  = std::function<void(Blackboard&)>;
        using Callbacks = Vector<Callback>;

        explicit Application(const AppDescriptor& descriptor);
        explicit Application(const Application&) = delete;
        explicit Application(Application&&)      = delete;
        virtual ~Application();

        void run();

        // bind an application bundle that will run with application loop
        template <typename T>
        void bind(T& bundle)
        {
            bundle.bind(*this);
        }

        // bind individual functions that will run with application loop
        template <AppEvent E>
        void bind(Callback&& callback)
        {
            callbacks.at(static_cast<uint>(E)).push_back(callback);
        }

        // bind class member function with class instance
        template <AppEvent E, typename F, typename T>
        void bind(F&& f, T* user)
        {
            static_assert(function_traits<F>::arity <= 1, "Bound function can at most take 1 argument with type Blackboard&");

            if constexpr (function_traits<F>::arity == 0) {
                bind<E>([user, f](Blackboard&) { return ((*user).*f)(); });
                return;
            }

            if constexpr (function_traits<F>::arity == 1) {
                bind<E>([user, f](Blackboard& blackboard) { return ((*user).*f)(blackboard); });
                return;
            }
        }

        auto& get_blackboard() { return blackboard; }
        auto& get_blackboard() const { return blackboard; }

        auto& get_window_descriptor() const { return descriptor.wsi; }
        auto& get_graphics_descriptor() const { return descriptor.rhi; }
        auto& get_compiler_descriptor() const { return descriptor.slc; }

    private:
        void init();
        void update();
        void render();
        void resize();
        void destroy();

    private:
        void init_logger();
        void init_window();
        void init_graphics();
        void init_compiler();
        void bind_events();

        template <AppEvent E>
        void run_callbacks()
        {
            uint  index = static_cast<uint>(E);
            auto& funcs = callbacks.at(index);
            for (auto& cb : funcs)
                cb(blackboard);
        }

    private:
        AppDescriptor descriptor;
        Blackboard    blackboard;

        OwnedResource<Window>   wsi;
        OwnedResource<RHI>      rhi;
        OwnedResource<Compiler> slc;
        GPUDevice               device;
        GPUAdapter              adapter;
        GPUSurface              surface;

        Array<Callbacks, STAGE_COUNT> callbacks;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_APPLET_APPLICATION_H
