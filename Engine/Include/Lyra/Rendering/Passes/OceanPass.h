#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_OCEAN_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_OCEAN_PASS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/PipelineManager.h>

namespace lyra::passes
{
    /**
     * @brief Gerstner wave parameter descriptor (16-byte aligned).
     */
    struct alignas(16) OceanWaveData
    {
        Vector2 direction  = Vector2(1.0f, 0.0f);
        float   amplitude  = 0.40f;
        float   wavelength = 20.0f;
        float   steepness  = 0.35f;
        float   phase      = 0.0f;
        Vector2 padding    = Vector2(0.0f);
    };

    /**
     * @brief Runtime parameters and inputs for rendering the ocean surface.
     */
    struct OceanPassParams
    {
        FrameGraphHandle<FrameGraphTexture> color_target;
        FrameGraphHandle<FrameGraphTexture> depth_target;
        RenderView                          view;
        RenderScene                         scene;
        float                               time          = 0.0f;
        float                               water_depth   = 15.0f;
        float                               water_clarity = 1.0f;
        float                               wave_speed    = 1.0f;
        float                               roughness     = 0.06f;
        float                               foam_scale    = 1.0f;
        Vector4                             deep_color    = Vector4(0.012f, 0.055f, 0.13f, 1.0f);
        Vector4                             shallow_color = Vector4(0.045f, 0.28f, 0.36f, 1.0f);
        Array<OceanWaveData, 4>             waves         = []() {
            Array<OceanWaveData, 4> w;
            w[0] = {Vector2(0.85f, 0.52f), 0.40f, 24.0f, 0.35f, 0.0f};
            w[1] = {Vector2(0.55f, -0.83f), 0.25f, 14.0f, 0.30f, 1.4f};
            w[2] = {Vector2(-0.42f, 0.90f), 0.14f, 6.5f, 0.42f, 2.8f};
            w[3] = {Vector2(0.92f, -0.38f), 0.07f, 2.8f, 0.48f, 0.9f};
            return w;
        }();
        GPULoadOp color_load_op = GPULoadOp::LOAD;
        GPULoadOp depth_load_op = GPULoadOp::LOAD;
    };

    /**
     * @brief Output handles of the ocean pass.
     */
    struct OceanPassData
    {
        FrameGraphHandle<FrameGraphTexture> color_output;
        FrameGraphHandle<FrameGraphTexture> depth_output;
    };

    /**
     * @brief Composable Gerstner ocean surface pass with physical optics,
     * sun specular glint, wave crest foam accumulation, and depth extinction.
     */
    struct OceanPass
    {
    public:
        explicit OceanPass() = default;
        virtual ~OceanPass() = default;

        OceanPass(const OceanPass&)            = delete;
        OceanPass& operator=(const OceanPass&) = delete;

        OceanPass(OceanPass&&) noexcept            = default;
        OceanPass& operator=(OceanPass&&) noexcept = default;

        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat color_format   = GPUTextureFormat::RGBA8UNORM,
            GPUTextureFormat depth_format   = GPUTextureFormat::DEPTH32FLOAT,
            uint             grid_subdivs_x = 128,
            uint             grid_subdivs_z = 128,
            float            grid_width     = 80.0f,
            float            grid_depth     = 80.0f);

        void destroy();

        auto add(FrameGraphBuilder& builder, const OceanPassParams& params) -> OceanPassData;

        bool valid() const { return initialized && pipeline.valid(); }

    private:
        GPUDevice      device;
        PipelineBundle pipeline;

        GPUBuffer grid_vbuffer;
        GPUBuffer grid_ibuffer;
        uint      grid_index_count = 0;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };
} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_OCEAN_PASS_H
