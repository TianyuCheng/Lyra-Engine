#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RendererTypes.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the tonemapping pass.
     */
    struct TonemappingData
    {
        FrameGraphHandle<FrameGraphTexture> hdr_input;
        FrameGraphHandle<FrameGraphTexture> ldr_output;
    };

    /**
     * @brief Tonemapping pass maps high dynamic range (HDR) radiance values
     * to standard low dynamic range (LDR) display color space with exposure and gamma correction.
     */
    struct TonemappingPass
    {
    public:
        explicit TonemappingPass() = default;
        ~TonemappingPass()         = default;

        TonemappingPass(const TonemappingPass&)            = delete;
        TonemappingPass& operator=(const TonemappingPass&) = delete;

        TonemappingPass(TonemappingPass&&) noexcept            = default;
        TonemappingPass& operator=(TonemappingPass&&) noexcept = default;

        /**
         * @brief initializes tonemapping pipeline and sampler.
         */
        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat output_format = GPUTextureFormat::RGBA8UNORM);

        /**
         * @brief safely destroys GPU pipeline and sampler states.
         */
        void destroy();

        /**
         * @brief records the tonemapping pass into the frame graph builder.
         */
        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> hdr_input,
            const RendererSettings&             settings,
            GPUTextureFormat                    output_format = GPUTextureFormat::RGBA8UNORM) -> TonemappingData;

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

    private:
        GPUDevice                        device;
        GPURenderPipeline                pipeline;
        GPUPipelineLayout                playout;
        GPUShaderModule                  vshader;
        GPUShaderModule                  fshader;
        GPUSampler                       sampler;
        GPUBindGroupHeap                 heap;
        GPUBindGroup                     bind_group;
        Vector<GPUBindGroupLayoutHandle> blayouts;

        GPUTextureFormat cached_output_format = GPUTextureFormat::RGBA8UNORM;
        bool             initialized          = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H
