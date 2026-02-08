#pragma once

#ifndef LYRA_LIBRARY_WINDOW_WSI_TYPES_H
#define LYRA_LIBRARY_WINDOW_WSI_TYPES_H

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Window/WSIEnums.h>
#include <Lyra/Window/WSIDescs.h>
#include <Lyra/Window/WSIUtils.h>
#include <Lyra/Window/WSIEvent.h>
#include <Lyra/Window/WSIState.h>

namespace lyra
{
    struct Window;
    struct WindowAPI;

    // clang-format off
    template <typename>   struct is_window_callback                                   : std::false_type {};
    template <>           struct is_window_callback<void(*)(const Window&)>           : std::true_type  {};
    template <typename C> struct is_window_callback<void (C::*)(const Window&)>       : std::true_type  {};
    template <typename C> struct is_window_callback<void (C::*)(const Window&) const> : std::true_type  {};
    // clang-format on

    struct Window
    {
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

        template <WindowEvent E, auto F>
        std::enable_if_t<is_window_callback<decltype(F)>::value, void> bind()
        {
            auto cb = WindowDelegate::create<F>();
            return bind<E>(cb);
        }

        template <WindowEvent E, auto F, typename Class>
        std::enable_if_t<is_window_callback<decltype(F)>::value, void> bind(Class& instance)
        {
            auto cb = WindowDelegate::create<Class, F>(instance);
            return bind<E>(cb);
        }

        template <WindowEvent E>
        void bind(WindowDelegate f)
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
        void dispatch(WindowEvent event);

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

#endif // LYRA_LIBRARY_WINDOW_WSI_TYPES_H
