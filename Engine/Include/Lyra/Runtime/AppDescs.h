#pragma once

#ifndef LYRA_ENGINE_RUNTIME_APP_DESCS_H
#define LYRA_ENGINE_RUNTIME_APP_DESCS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Windowing/WSIDescs.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Compiler/SLCDescs.h>
#include <Lyra/JobSystem/Jobs.h>

namespace lyra
{
    struct Application;

    using AppWindowDescriptor = WindowDescriptor;
    using AppJobDescriptor    = JobSystemDescriptor;

    /**
     * @brief Graphics configuration for the application.
     */
    struct AppGraphicsDescriptor
    {
        RHIBackend backend;    ///< The rendering backend (e.g., Vulkan, D3D12).
        RHIFlags   flags;      ///< RHI initialization flags.
        uint       frames = 3; ///< Number of frames in flight.
    };

    /**
     * @brief Shader compiler configuration for the application.
     */
    struct AppCompilerDescriptor
    {
        CompileTarget target; ///< The shader compilation target (e.g., SPIR-V, DXIL).
        CompileFlags  flags;  ///< Compiler flags.
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
        /**
         * @brief Set the worker concurrency for the job system (0 = auto-detect).
         */
        AppDescriptor& with_workers(uint workers);

    private:
        AppWindowDescriptor   wsi;
        AppGraphicsDescriptor rhi;
        AppCompilerDescriptor slc;
        JobSystemDescriptor   jobs;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_APP_DESCS_H
