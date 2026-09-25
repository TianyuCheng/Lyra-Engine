#pragma once

#ifndef LYRA_ENGINE_RUNTIME_APPLICATION_H
#define LYRA_ENGINE_RUNTIME_APPLICATION_H

#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Runtime/AppEnums.h>
#include <Lyra/Runtime/AppDescs.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Graphics/RHITypes.h>

namespace lyra
{
    /**
     * @brief The main application class that manages the main loop, window, and RHI.
     */
    struct Application
    {
        static constexpr size_t STAGE_COUNT = magic_enum::enum_count<AppEvent>();

    public:
        using Callback  = Delegate<void(AppContext&)>;
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
         * @brief Get the application execution context.
         */
        auto& get_context() { return context; }
        auto& get_context() const { return context; }

        /**
         * @brief Get the application toolboard for physical devices and subsystems.
         */
        auto& get_toolboard() { return context.toolboard; }
        auto& get_toolboard() const { return context.toolboard; }

        /**
         * @brief Get the application blackboard for global data sharing.
         */
        auto& get_blackboard() { return context.blackboard; }
        auto& get_blackboard() const { return context.blackboard; }

        /**
         * @brief Get window and graphics descriptors.
         */
        auto& get_window_descriptor() const { return descriptor.wsi; }
        auto& get_graphics_descriptor() const { return descriptor.rhi; }
        auto& get_compiler_descriptor() const { return descriptor.slc; }
        auto& get_job_system_descriptor() const { return descriptor.jobs; }
        auto  get_max_workers() const -> uint { return descriptor.jobs.max_workers; }
        auto  get_max_background_workers() const -> uint { return descriptor.jobs.max_background_workers; }

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
        void init_job_system();
        void bind_events();

        template <AppEvent E>
        void run_callbacks()
        {
            uint  index = static_cast<uint>(E);
            auto& funcs = callbacks.at(index);
            for (auto& cb : funcs)
                cb(context);
        }

    private:
        AppDescriptor descriptor;
        AppContext    context;

        OwnedResource<Window>   wsi;
        OwnedResource<RHI>      rhi;
        OwnedResource<Compiler> slc;
        GPUDevice               device;
        GPUAdapter              adapter;
        GPUSurface              surface;

        Array<Callbacks, STAGE_COUNT> callbacks;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_APPLICATION_H
