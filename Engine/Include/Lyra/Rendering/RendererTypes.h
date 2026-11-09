#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDERER_TYPES_H
#define LYRA_ENGINE_RENDERING_RENDERER_TYPES_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>

namespace lyra
{
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
    struct RendererCaps
    {
        RendererFeatures  features             = RendererFeature::NONE;
        PrimitiveSupports supported_primitives = PrimitiveSupport::NONE;
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
        GPUTextureFormat color_format = GPUTextureFormat::BGRA8UNORM;
        GPUTextureFormat depth_format = GPUTextureFormat::DEPTH24PLUS_STENCIL8;
        GPUExtent2D      initial_extent{1280, 720};
    };

    struct RendererInstance;
    using RendererHandle = TypedPointerHandle<RendererInstance>;

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDERER_TYPES_H
