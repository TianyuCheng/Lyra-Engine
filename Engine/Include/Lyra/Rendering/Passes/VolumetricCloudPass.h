#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_VOLUMETRIC_CLOUD_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_VOLUMETRIC_CLOUD_PASS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Components/Cloudscape.h>
#include <Lyra/Rendering/Pipeline.h>

namespace lyra::passes
{
    /**
     * @brief Runtime parameters and inputs for rendering volumetric cloudscapes.
     */
    struct VolumetricCloudPassParams
    {
        FrameGraphHandle<FrameGraphTexture> color_target;
        FrameGraphHandle<FrameGraphTexture> depth_target;
        RenderView                          view;
        RenderScene                         scene;

        // Cloud simulation & shape
        float   bottom_altitude    = 1.5f;                      // inner boundary altitude in km
        float   top_altitude       = 4.0f;                      // outer boundary altitude in km
        float   planet_radius      = 6360.0f;                   // planetary shell radius in km
        float   max_march_distance = 50.0f;                     // raymarch clipping distance in km
        float   coverage           = 0.48f;                     // global cloud coverage fraction in [0, 1]
        float   density_scale      = 0.55f;                     // extinction multiplier
        float   cloud_type         = 0.7f;                      // 0=Stratus, 0.5=Stratocumulus, 1=Cumulus
        float   shape_scale        = 0.35f;                     // base noise frequency
        float   detail_scale       = 2.8f;                      // high-frequency erosion frequency
        float   detail_strength    = 0.35f;                     // erosion influence in [0, 1]
        float   curl_strength      = 0.15f;                     // curl distortion weight
        Vector3 wind_direction     = Vector3(1.0f, 0.0f, 0.4f); // normalized horizontal wind
        float   wind_speed         = 0.04f;                     // wind speed in km/s
        float   height_shear       = 0.3f;                      // altitude-dependent wind shear
        float   time               = 0.0f;                      // simulation elapsed time

        // Scattering & lighting
        float   multi_scatter_boost  = 0.5f;   // multiple scattering energy factor
        float   powder_scale         = 1.5f;   // Beer-Powder dark border factor
        float   phase_g_forward      = 0.82f;  // forward Mie asymmetry parameter
        float   phase_g_backward     = -0.22f; // backward Mie asymmetry parameter
        float   phase_blend          = 0.72f;  // forward lobe blend weight
        Vector3 sun_color            = Vector3(1.0f, 0.98f, 0.92f);
        float   sun_intensity        = 20.0f;
        Vector3 ambient_sky_color    = Vector3(0.35f, 0.45f, 0.65f);
        Vector3 ambient_ground_color = Vector3(0.12f, 0.11f, 0.09f);
        float   ambient_intensity    = 1.2f;

        // Raymarching execution
        uint      max_steps      = 32;    // raymarching sample count
        bool      enable_tonemap = false; // if true, tonemaps output (for standalone rendering)
        GPULoadOp color_load_op  = GPULoadOp::LOAD;
    };

    /**
     * @brief Output handles of the volumetric cloud pass.
     */
    struct VolumetricCloudPassData
    {
        FrameGraphHandle<FrameGraphTexture> color_output;
    };

    /**
     * @brief Composable Real-Time Volumetric Cloudscape pass (Milestone 3, Task 3.4).
     * Implements multi-octave Perlin-Worley density modeling, Schneider dual-lobe
     * forward silver-lining phase function, Beer-Powder dark border crevasse shading,
     * and analytical multiple scattering approximation boost.
     */
    struct VolumetricCloudPass
    {
    public:
        explicit VolumetricCloudPass() = default;
        virtual ~VolumetricCloudPass() = default;

        VolumetricCloudPass(const VolumetricCloudPass&)            = delete;
        VolumetricCloudPass& operator=(const VolumetricCloudPass&) = delete;

        VolumetricCloudPass(VolumetricCloudPass&&) noexcept            = default;
        VolumetricCloudPass& operator=(VolumetricCloudPass&&) noexcept = default;

        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat color_format = GPUTextureFormat::RGBA8UNORM);

        void destroy();

        auto add(FrameGraphBuilder& builder, const VolumetricCloudPassParams& params) -> VolumetricCloudPassData;

        bool valid() const { return initialized && pipeline.valid(); }

    private:
        GPUDevice      device;
        PipelineBundle pipeline;
        GPUSampler     linear_sampler;
        GPUTexture     default_depth_tex;
        GPUTextureView default_depth_view;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        bool             initialized         = false;
    };
} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_VOLUMETRIC_CLOUD_PASS_H
