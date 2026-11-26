#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDERER_TYPES_H
#define LYRA_ENGINE_RENDERING_RENDERER_TYPES_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>

namespace lyra
{
    class SystemTextures;

    /**
     * @brief available renderer backend implementations.
     */
    enum struct RendererBackend : uint
    {
        FORWARD,
        DEFERRED,
        GPU_DRIVEN,
        PATH_TRACER,
    };

    constexpr auto to_string(RendererBackend backend) -> CString
    {
        switch (backend) {
            case RendererBackend::FORWARD:
                return "Forward";
            case RendererBackend::DEFERRED:
                return "Deferred";
            case RendererBackend::GPU_DRIVEN:
                return "GPUDriven";
            case RendererBackend::PATH_TRACER:
                return "PathTracer";
        }
        return "Unknown";
    }

    constexpr auto get_renderer_plugin_name(RendererBackend backend) -> CString
    {
        switch (backend) {
            case RendererBackend::FORWARD:
                return "lyra-forward";
            case RendererBackend::DEFERRED:
                return "lyra-deferred";
            case RendererBackend::GPU_DRIVEN:
                return "lyra-gpudriven";
            case RendererBackend::PATH_TRACER:
                return "lyra-pathtracer";
        }
        return "";
    }

    inline auto to_renderer_backend(StringView name) -> Optional<RendererBackend>
    {
        if (name == "lyra-forward" || name == "forward" || name == "Forward" || name == "FORWARD") {
            return RendererBackend::FORWARD;
        }
        if (name == "lyra-deferred" || name == "deferred" || name == "Deferred" || name == "DEFERRED") {
            return RendererBackend::DEFERRED;
        }
        if (name == "lyra-gpudriven" || name == "gpudriven" || name == "GPUDriven" || name == "gpu_driven" || name == "GPU_DRIVEN") {
            return RendererBackend::GPU_DRIVEN;
        }
        if (name == "lyra-pathtracer" || name == "pathtracer" || name == "PathTracer" || name == "path_tracer" || name == "PATH_TRACER") {
            return RendererBackend::PATH_TRACER;
        }
        return std::nullopt;
    }

    /**
     * @brief capabilities and supported features of a renderer.
     */
    enum struct RendererFeature : uint
    {
        NONE                = 0,
        RASTERIZATION       = 1 << 0,
        COMPUTE_CULLING     = 1 << 1,
        MESH_SHADERS        = 1 << 2,
        HARDWARE_RAYTRACING = 1 << 3,
        ACCUMULATION        = 1 << 4,
        POST_PROCESSING     = 1 << 5,
        DEBUG_VISUALIZATION = 1 << 6,
    };

    /**
     * @brief geometric primitive presentations supported by a renderer.
     */
    enum struct PrimitiveSupport : uint
    {
        NONE    = 0,
        MESH    = 1 << 0, ///< traditional indexed vbo/ibo
        MESHLET = 1 << 1, ///< meshlet / cluster geometry
        SDF     = 1 << 2, ///< signed distance field analytic / volume
        BVH     = 1 << 3, ///< bottom/top level acceleration structures
    };
} // namespace lyra

ENABLE_BIT_FLAGS(lyra::RendererFeature)
ENABLE_BIT_FLAGS(lyra::PrimitiveSupport)

namespace lyra
{
    using RendererFeatures  = BitFlags<RendererFeature>;
    using PrimitiveSupports = BitFlags<PrimitiveSupport>;

    /**
     * @brief reported capabilities of a renderer plugin.
     */
    struct RendererCapabilities
    {
        RendererFeatures  features               = RendererFeature::NONE;
        PrimitiveSupports supported_primitives   = PrimitiveSupport::NONE;
        uint              max_directional_lights = 4;
        uint              max_local_lights       = 4096;
        uint              max_samples_per_pixel  = 1;
        bool              supports_hdr           = true;
    };

    /**
     * @brief runtime user/editor settings for the active renderer.
     */
    struct RendererSettings
    {
        bool  enable_shadows      = true;
        bool  enable_ambient_occl = true;
        bool  enable_reflections  = true;
        bool  enable_wireframe    = false;
        uint  max_ray_bounces     = 4;
        uint  samples_per_pixel   = 1;
        float exposure            = 1.0f;
        uint  debug_mode          = 0; // 0 = lit, 1 = normal, 2 = albedo, 3 = wireframe, 4 = heatmaps
        bool  enable_denoiser     = false;
        uint  denoiser_iterations = 4;
    };

    /**
     * @brief runtime rendering telemetry and profiling statistics.
     */
    struct RendererStats
    {
        uint  draw_calls         = 0;
        uint  dispatch_calls     = 0;
        uint  triangles_rendered = 0;
        uint  meshlets_rendered  = 0;
        uint  rays_traced        = 0;
        float gpu_time_ms        = 0.0f;
        float cpu_prep_time_ms   = 0.0f;
    };

    /**
     * @brief initialization descriptor for a renderer instance.
     */
    struct RendererDescriptor
    {
        GPUDevice        device;
        Compiler         compiler;
        SystemTextures*  system_textures = nullptr;
        GPUTextureFormat color_format    = GPUTextureFormat::BGRA8UNORM;
        GPUTextureFormat depth_format    = GPUTextureFormat::DEPTH32FLOAT;
        GPUExtent2D      initial_extent{1280, 720};
    };

    /**
     * @brief temporal state for procedural animations, stochastic sampling, and TAA/FSR.
     */
    struct RenderFrameTime
    {
        float delta_time  = 0.0f;
        float total_time  = 0.0f;
        ulong frame_index = 0;
    };

    struct RendererInstance;
    using RendererHandle = TypedPointerHandle<RendererInstance>;

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDERER_TYPES_H
