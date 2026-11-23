#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_SKYBOX_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_SKYBOX_PASS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the skybox / environment pass.
     */
    struct SkyboxData
    {
        FrameGraphHandle<FrameGraphTexture> color_output;
        FrameGraphHandle<FrameGraphTexture> depth_input;
        FrameGraphHandle<FrameGraphTexture> sky_view_input;
    };

    /**
     * @brief Composable physical atmospheric skybox pass.
     * Implements precomputed 3-LUT atmospheric scattering architecture
     * (Hillaire 2020 / Bruneton 2008 / ROADMAP Milestone 3, Task 3.1.1):
     * 1. TransmittanceLUT (256x64, optical depth & solar beam transmittance)
     * 2. MultiScatteringLUT (32x32, 2nd-order scattering transfer)
     * 3. SkyViewLUT (192x108, non-linear horizon-packed spherical coordinates)
     */
    struct SkyboxPass
    {
    public:
        explicit SkyboxPass() = default;
        virtual ~SkyboxPass() = default;

        SkyboxPass(const SkyboxPass&)            = delete;
        SkyboxPass& operator=(const SkyboxPass&) = delete;

        SkyboxPass(SkyboxPass&&) noexcept            = default;
        SkyboxPass& operator=(SkyboxPass&&) noexcept = default;

        /**
         * @brief initialize skybox and atmospheric LUT pipelines, layouts, textures, and camera uniform buffers.
         */
        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat color_format = GPUTextureFormat::RGBA8UNORM,
            GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT);

        /**
         * @brief safely release GPU pipelines, LUT textures, buffers, and shader modules.
         */
        void destroy();

        /**
         * @brief record the skybox pass and Sky-View LUT compute pass into the frame graph builder.
         */
        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> color_target,
            FrameGraphHandle<FrameGraphTexture> depth_target,
            const RenderView&                   view,
            const RenderScene&                  scene,
            GPULoadOp                           depth_load_op = GPULoadOp::LOAD,
            GPULoadOp                           color_load_op = GPULoadOp::LOAD) -> SkyboxData;

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

    private:
        void update_camera_uniform(uint slot, const RenderView& view, const RenderScene& scene);
        void precompute_static_luts();

    private:
        static constexpr uint FRAMES_IN_FLIGHT = 3;

        GPUDevice                          device;
        GPURenderPipeline                  pipeline;
        GPUPipelineLayout                  playout;
        GPUShaderModule                    vshader;
        GPUShaderModule                    fshader;
        Array<GPUBuffer, FRAMES_IN_FLIGHT> camera_buffers{};
        uint                               frame_slot = 0;
        GPUBindGroupHeap                   heap;
        Vector<GPUBindGroupLayoutHandle>   blayouts;

        // 3-LUT Atmospheric Scattering Pipeline (Milestone 3, Task 3.1.1)
        // 1. Transmittance LUT (256x64)
        GPUTexture                       transmittance_lut_tex;
        GPUTextureView                   transmittance_lut_view;
        GPURenderPipeline                transmittance_pipeline;
        GPUPipelineLayout                transmittance_playout;
        GPUShaderModule                  transmittance_vshader;
        GPUShaderModule                  transmittance_fshader;
        Vector<GPUBindGroupLayoutHandle> transmittance_blayouts;

        // 2. Multi-Scattering LUT (32x32)
        GPUTexture                       multiscatter_lut_tex;
        GPUTextureView                   multiscatter_lut_view;
        GPURenderPipeline                multiscatter_pipeline;
        GPUPipelineLayout                multiscatter_playout;
        GPUShaderModule                  multiscatter_vshader;
        GPUShaderModule                  multiscatter_fshader;
        Vector<GPUBindGroupLayoutHandle> multiscatter_blayouts;
        GPUBindGroup                     multiscatter_bind_group;

        // 3. Sky-View LUT (192x108)
        GPURenderPipeline                     sky_view_pipeline;
        GPUPipelineLayout                     sky_view_playout;
        GPUShaderModule                       sky_view_vshader;
        GPUShaderModule                       sky_view_fshader;
        Vector<GPUBindGroupLayoutHandle>      sky_view_blayouts;
        Array<GPUBindGroup, FRAMES_IN_FLIGHT> sky_view_bind_groups{};

        GPUSampler linear_sampler;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_SKYBOX_PASS_H
