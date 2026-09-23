#pragma once

#ifndef LYRA_ENGINE_RUNTIME_APP_TYPES_H
#define LYRA_ENGINE_RUNTIME_APP_TYPES_H

#include <type_traits>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>

namespace lyra
{
    struct Application;

    /**
     * @brief Unified application execution context passed to lifecycle callbacks and layers.
     *
     * Groups:
     * - `toolboard`: Non-owning registry for physical hardware, devices, and subsystems
     *   (e.g., GPUDevice, Window, Compiler, World).
     * - `blackboard`: Owning registry for dynamic state, transient frame data, and parameters.
     */
    struct AppContext
    {
        Toolboard  toolboard;
        Blackboard blackboard;

        // Convenient accessors for tools / subsystems
        template <typename T>
        FORCE_INLINE decltype(auto) tool() { return toolboard.get<T>(); }

        template <typename T>
        FORCE_INLINE decltype(auto) tool() const { return toolboard.get<T>(); }

        template <typename T>
        FORCE_INLINE auto try_tool() { return toolboard.try_get<T>(); }

        template <typename T>
        FORCE_INLINE auto try_tool() const { return toolboard.try_get<T>(); }

        // Convenient accessors for blackboard data
        template <typename T>
        FORCE_INLINE decltype(auto) data() { return blackboard.get<T>(); }

        template <typename T>
        FORCE_INLINE decltype(auto) data() const { return blackboard.get<T>(); }

        template <typename T>
        FORCE_INLINE auto try_data() { return blackboard.try_get<T>(); }

        template <typename T>
        FORCE_INLINE auto try_data() const { return blackboard.try_get<T>(); }
    };

    /**
     * @brief Type trait to check if a function is a valid application callback.
     */
    // clang-format off
    template <typename>   struct is_app_callback                                 : std::false_type {};
    template <>           struct is_app_callback<void(*)(AppContext&)>           : std::true_type  {};
    template <typename C> struct is_app_callback<void (C::*)(AppContext&)>       : std::true_type  {};
    template <typename C> struct is_app_callback<void (C::*)(AppContext&) const> : std::true_type  {};
    // clang-format on

    /**
     * @brief Represents the swapchain backbuffer of the application.
     */
    struct Backbuffer
    {
        GPUTextureHandle     texture; ///< Handle to the backbuffer texture.
        GPUTextureViewHandle texview; ///< Handle to the texture view.
        GPUTextureFormat     format;  ///< Format of the backbuffer.
        GPUExtent2D          extent;  ///< Dimensions of the backbuffer.
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_APP_TYPES_H
