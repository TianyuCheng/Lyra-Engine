#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Utilities/Path.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the cascaded shadow mapping pass.
     */
    struct ShadowMapData
    {
        FrameGraphHandle<FrameGraphTexture> shadow_map_array;
    };

    /**
     * @brief Cascaded shadow mapping pass renders scene geometry from the directional light's perspective
     * into layered shadow map cascades.
     */
    struct ShadowMapPass
    {
    public:
        explicit ShadowMapPass() = default;
        virtual ~ShadowMapPass() = default;

        ShadowMapPass(const ShadowMapPass&)            = delete;
        ShadowMapPass& operator=(const ShadowMapPass&) = delete;

        ShadowMapPass(ShadowMapPass&&) noexcept            = default;
        ShadowMapPass& operator=(ShadowMapPass&&) noexcept = default;

        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT);

        void destroy();

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

        auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            uint               cascade_count = 4,
            uint               resolution    = 2048) -> ShadowMapData;

    private:
        struct ShadowCascadeData
        {
            Matrix4x4 light_view_proj;
        };

        struct alignas(16) InstanceData
        {
            Matrix4x4 model;
            uint      pos_offset = 0;
            uint      padding0   = 0;
            uint      padding1   = 0;
            uint      padding2   = 0;
        };
        static_assert(sizeof(InstanceData) == 80);
        static_assert(offsetof(InstanceData, pos_offset) == 64);

        static constexpr uint FRAMES_IN_FLIGHT = 3;

        GPUDevice                             device;
        GPURenderPipeline                     pipeline;
        GPUPipelineLayout                     playout;
        GPUShaderModule                       vshader;
        GPUShaderModule                       fshader;
        Vector<GPUBindGroupLayoutHandle>      blayouts;
        GPUBindGroupHeap                      heap;
        Array<GPUBuffer, FRAMES_IN_FLIGHT>    cascade_uniform_buffers{};
        Array<GPUBindGroup, FRAMES_IN_FLIGHT> bind_groups{};
        uint                                  frame_slot = 0;
        HashMap<uint64_t, GPUBindGroup>       geometry_bind_groups;
        GPUTextureFormat                      cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool                                  initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H
