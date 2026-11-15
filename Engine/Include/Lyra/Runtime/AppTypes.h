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
     * Holds:
     * - `services`: Non-owning service locator for physical hardware, devices, and subsystems
     *   (e.g., GPUDevice, Window, Compiler, World, AssetServer).
     */
    struct AppContext
    {
        Services services;

        // Convenient accessors for services / subsystems
        template <typename T>
        FORCE_INLINE decltype(auto) service() { return services.get<T>(); }

        template <typename T>
        FORCE_INLINE decltype(auto) service() const { return services.get<T>(); }

        template <typename T>
        FORCE_INLINE auto try_service() { return services.try_get<T>(); }

        template <typename T>
        FORCE_INLINE auto try_service() const { return services.try_get<T>(); }
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
