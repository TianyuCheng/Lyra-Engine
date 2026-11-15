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
    };

    /**
     * @brief Composable skybox / environment pass.
     * Renders background atmospheric sky radiance or cubemap at depth = 1.0 (far plane).
     * Depth compare is LESS_EQUAL or EQUAL with depth_write_enabled = false,
     * so only untouched background pixels receive skybox radiance.
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
         * @brief initialize skybox pipeline, layouts, and camera uniform buffer.
         */
        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat color_format = GPUTextureFormat::RGBA8UNORM,
            GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT);

        /**
         * @brief safely release GPU pipeline, buffers, and shader modules.
         */
        void destroy();

        /**
         * @brief record the skybox pass into the frame graph builder.
         */
        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> color_target,
            FrameGraphHandle<FrameGraphTexture> depth_target,
            const RenderView&                   view,
            const RenderScene&                  scene) -> SkyboxData;

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

    private:
        void update_camera_uniform(const RenderView& view, const RenderScene& scene);

    private:
        GPUDevice                        device;
        GPURenderPipeline                pipeline;
        GPUPipelineLayout                playout;
        GPUShaderModule                  vshader;
        GPUShaderModule                  fshader;
        GPUBuffer                        camera_buffer;
        GPUBindGroupHeap                 heap;
        GPUBindGroup                     bind_group;
        Vector<GPUBindGroupLayoutHandle> blayouts;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_SKYBOX_PASS_H
