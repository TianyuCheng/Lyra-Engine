#pragma once

#ifndef LYRA_LYRA_PLAYER_APPLICATION_H
#define LYRA_LYRA_PLAYER_APPLICATION_H

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Window/WSITypes.h>
#include <Lyra/Shader/SLCTypes.h>
#include <Lyra/Render/RHITypes.h>

namespace lyra
{
    using AppWindowDescriptor = WindowDescriptor;

    /**
     * @brief Type trait to check if a function is a valid application callback.
     */
    // clang-format off
    template <typename>   struct is_app_callback                                 : std::false_type {};
    template <>           struct is_app_callback<void(*)(Blackboard&)>           : std::true_type  {};
    template <typename C> struct is_app_callback<void (C::*)(Blackboard&)>       : std::true_type  {};
    template <typename C> struct is_app_callback<void (C::*)(Blackboard&) const> : std::true_type  {};
    // clang-format on

    /**
     * @brief Application lifecycle events.
     */
    enum struct AppEvent : uint
    {
        INIT,           ///< Called during application initialization.
        DESTROY,        ///< Called during application destruction.
        RESIZE,         ///< Called when the application window is resized.

        UPDATE,         ///< Main update logic.
        UPDATE_PRE,     ///< Logic executed before the main update.
        UPDATE_POST,    ///< Logic executed after the main update.
        UPDATE_FIXED,   ///< Fixed-rate update logic.

        UI,             ///< UI rendering logic.
        UI_PRE,         ///< Logic executed before UI rendering.
        UI_POST,        ///< Logic executed after UI rendering.

        RENDER,         ///< Main rendering logic.
        RENDER_PRE,     ///< Logic executed before rendering.
        RENDER_POST,    ///< Logic executed after rendering.
    };

    /**
     * @brief Graphics configuration for the application.
     */
    struct AppGraphicsDescriptor
    {
        RHIBackend backend;     ///< The rendering backend (e.g., Vulkan, D3D12).
        RHIFlags   flags;       ///< RHI initialization flags.
        uint       frames = 3;  ///< Number of frames in flight.
    };

    /**
     * @brief Shader compiler configuration for the application.
     */
    struct AppCompilerDescriptor
    {
        CompileTarget target;   ///< The shader compilation target (e.g., SPIR-V, DXIL).
        CompileFlags  flags;    ///< Compiler flags.
    };

    /**
     * @brief Main application descriptor used for initialization.
     */
    struct AppDescriptor
    {
        friend struct Application;

    public:
        /**
         * @brief Set the window title.
         */
        AppDescriptor& with_title(CString title);
        /**
         * @brief Enable or disable fullscreen mode.
         */
        AppDescriptor& with_fullscreen(bool enable = true);
        /**
         * @brief Set the window to be maximized on startup.
         */
        AppDescriptor& with_window_maximized();
        /**
         * @brief Set the initial window size.
         */
        AppDescriptor& with_window_extent(uint width, uint height);
        /**
         * @brief Set the preferred graphics backend.
         */
        AppDescriptor& with_graphics_backend(RHIBackend backend);
        /**
         * @brief Enable graphics debugging and validation layers.
         */
        AppDescriptor& with_graphics_validation(bool debug = true, bool validation = true);
        /**
         * @brief Set the number of frames in flight.
         */
        AppDescriptor& with_frames_in_flight(uint frames_in_flight);

    private:
        AppWindowDescriptor   wsi;
        AppGraphicsDescriptor rhi;
        AppCompilerDescriptor slc;
    };

    /**
     * @brief Represents the swapchain backbuffer of the application.
     */
    struct Backbuffer
    {
        GPUTextureHandle     texture;   ///< Handle to the backbuffer texture.
        GPUTextureViewHandle texview;   ///< Handle to the texture view.
        GPUTextureFormat     format;    ///< Format of the backbuffer.
        GPUExtent2D          extent;    ///< Dimensions of the backbuffer.
    };

    /**
     * @brief The main application class that manages the main loop, window, and RHI.
     */
    struct Application
    {
        static constexpr size_t STAGE_COUNT = magic_enum::enum_count<AppEvent>();

    public:
        using Callback  = Delegate<void(Blackboard&)>;
        using Callbacks = Vector<Callback>;

        /**
         * @brief Constructor that initializes the application from a descriptor.
         */
        explicit Application(const AppDescriptor& descriptor);
        explicit Application(const Application&) = delete;
        explicit Application(Application&&)      = delete;
        virtual ~Application();

        /**
         * @brief Starts the application main loop.
         */
        void run();

        /**
         * @brief Bind an application bundle that will run with the application loop.
         * @tparam T A type that implements a `bind(Application&)` method.
         */
        template <typename T>
        void bind(T& bundle)
        {
            bundle.bind(*this);
        }

        /**
         * @brief Bind an individual callback function to an application event.
         * @tparam E The event to bind to.
         */
        template <AppEvent E>
        void bind(Callback callback)
        {
            callbacks.at(static_cast<uint>(E)).push_back(callback);
        }

        /**
         * @brief Bind a static or free function to an application event.
         * @tparam E The event to bind to.
         * @tparam F The function pointer.
         */
        template <AppEvent E, auto F>
        std::enable_if_t<is_app_callback<decltype(F)>::value, void> bind()
        {
            auto cb = Callback::create<F>();
            return bind<E>(cb);
        }

        /**
         * @brief Bind a member function of a class instance to an application event.
         * @tparam E The event to bind to.
         * @tparam F The member function pointer.
         * @tparam Class The class type.
         */
        template <AppEvent E, auto F, typename Class>
        std::enable_if_t<is_app_callback<decltype(F)>::value, void> bind(Class& instance)
        {
            auto cb = Callback::create<Class, F>(instance);
            return bind<E>(cb);
        }

        /**
         * @brief Get the application blackboard for global data sharing.
         */
        auto& get_blackboard() { return blackboard; }
        auto& get_blackboard() const { return blackboard; }

        /**
         * @brief Get window and graphics descriptors.
         */
        auto& get_window_descriptor() const { return descriptor.wsi; }
        auto& get_graphics_descriptor() const { return descriptor.rhi; }
        auto& get_compiler_descriptor() const { return descriptor.slc; }

    private:
        void init(const Window&);
        void update(const Window&);
        void render(const Window&);
        void resize(const Window&);
        void destroy(const Window&);

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

#endif // LYRA_LYRA_PLAYER_APPLICATION_H
