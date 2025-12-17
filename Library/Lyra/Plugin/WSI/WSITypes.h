#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_WSI_TYPES_H
#define LYRA_LIBRARY_PLUGIN_WSI_TYPES_H

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Plugin/WSI/WSIEnums.h>
#include <Lyra/Plugin/WSI/WSIDescs.h>
#include <Lyra/Plugin/WSI/WSIUtils.h>
#include <Lyra/Plugin/WSI/WSIEvent.h>
#include <Lyra/Plugin/WSI/WSIState.h>

namespace lyra
{
    struct Window;
    struct WindowAPI;

    struct WindowCallbacks
    {
        Vector<std::function<void(const Window&)>> start;
        Vector<std::function<void(const Window&)>> close;
        Vector<std::function<void(const Window&)>> timer;
        Vector<std::function<void(const Window&)>> update;
        Vector<std::function<void(const Window&)>> render;
        Vector<std::function<void(const Window&)>> resize;
    };

    struct Window
    {
        using GeneralCallback  = std::function<void()>;
        using ExplicitCallback = std::function<void(const Window&)>;

        friend struct EventLoop;

        static auto init(const WindowDescriptor& descriptor) -> OwnedResource<Window>;

        static auto api() -> WindowAPI*;

        // implicit conversion
        operator WindowHandle() { return handle; }
        operator WindowHandle() const { return handle; }

        // NOTE: only a convenience method for launching window and run.
        // use EventLoop::bind(...) and EventLoop::run() for multiple windows
        void loop();

        void destroy();

        auto get_input_state() const -> const WindowInput& { return inputs; }

        void get_position(int& x, int& y) const;

        void get_extent(uint& width, uint& height) const;

        void get_content_scale(float& xscale, float& yscale) const;

        void get_framebuffer_scale(float& xscale, float& yscale) const;

        template <WindowEvent E, typename F, typename T>
        void bind(F&& f, T* user)
        {
            static_assert(function_traits<F>::arity <= 1, "Bound function can at most take 1 argument with type const Window&");

            if constexpr (function_traits<F>::arity == 0) {
                bind<E>([user, f](const Window& window) { return ((*user).*f)(); });
                return;
            }

            if constexpr (function_traits<F>::arity == 1) {
                bind<E>([user, f](const Window& window) { return ((*user).*f)(window); });
                return;
            }
        }

        template <WindowEvent E>
        void bind(GeneralCallback&& f)
        {
            bind<E>([f](const Window&) { f(); });
        }

        template <WindowEvent E>
        void bind(ExplicitCallback&& f)
        {
            if constexpr (E == WindowEvent::START) {
                callbacks.start.push_back(f);
            }

            if constexpr (E == WindowEvent::CLOSE) {
                callbacks.close.push_back(f);
            }

            if constexpr (E == WindowEvent::TIMER) {
                callbacks.timer.push_back(f);
            }

            if constexpr (E == WindowEvent::UPDATE) {
                callbacks.update.push_back(f);
            }

            if constexpr (E == WindowEvent::RENDER) {
                callbacks.render.push_back(f);
            }

            if constexpr (E == WindowEvent::RESIZE) {
                callbacks.resize.push_back(f);
            }
        }

    private:
        WindowCallbacks callbacks;
        WindowInput     inputs;
        WindowHandle    handle;
    };

    struct EventLoop
    {
        static void bind(Window& window);

        static void run();
    };

    using WSI = Window;

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_WSI_TYPES_H
