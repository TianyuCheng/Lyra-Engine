#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_EDITOR_GRID_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_EDITOR_GRID_PASS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>

namespace lyra::passes
{
    /**
     * @brief visual and geometric configuration options for the editor infinite grid.
     */
    struct EditorGridSettings
    {
        Vector4 color_fine   = Vector4(0.48f, 0.48f, 0.52f, 0.30f);
        Vector4 color_coarse = Vector4(0.72f, 0.72f, 0.76f, 0.60f);
        Vector4 axis_color_x = Vector4(0.88f, 0.25f, 0.25f, 0.90f);
        Vector4 axis_color_z = Vector4(0.25f, 0.50f, 0.90f, 0.90f);
        float   scale        = 1.0f;
        float   plane_height = 0.0f;
        bool    show_axes    = true;
        bool    show_subgrid = true;
    };

    /**
     * @brief outputs of the editor grid pass.
     */
    struct EditorGridData
    {
        FrameGraphHandle<FrameGraphTexture> color_output;
        FrameGraphHandle<FrameGraphTexture> depth_input;
    };

    /**
     * @brief Composable editor infinite grid pass.
     *
     * Renders a multi-level distance-faded infinite grid plane at Y = plane_height,
     * performing depth testing (LESS_EQUAL) against the scene's existing depth buffer
     * so opaque scene meshes properly occlude the grid without overwriting scene depth.
     */
    struct EditorGridPass
    {
    public:
        explicit EditorGridPass() = default;
        virtual ~EditorGridPass() = default;

        EditorGridPass(const EditorGridPass&)            = delete;
        EditorGridPass& operator=(const EditorGridPass&) = delete;

        EditorGridPass(EditorGridPass&&) noexcept            = default;
        EditorGridPass& operator=(EditorGridPass&&) noexcept = default;

        /**
         * @brief initializes pipeline states, layouts, and uniform buffers.
         * Safe to call multiple times (idempotent; rebuilds only on format changes).
         */
        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat color_format,
            GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT);

        /**
         * @brief safely destroys pipeline, layout, shader modules, and GPU buffers.
         */
        void destroy();

        /**
         * @brief records the grid pass into the provided frame graph builder.
         */
        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> color_target,
            FrameGraphHandle<FrameGraphTexture> depth_target,
            const RenderView&                   view,
            const EditorGridSettings&           settings = {}) -> EditorGridData;

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

    private:
        void update_camera_uniform(uint slot, const RenderView& view);

    private:
        static constexpr uint FRAMES_IN_FLIGHT = 3;

        GPUDevice                             device;
        GPURenderPipeline                     pipeline;
        GPUPipelineLayout                     playout;
        GPUShaderModule                       vshader;
        GPUShaderModule                       fshader;
        Array<GPUBuffer, FRAMES_IN_FLIGHT>    camera_buffers{};
        Array<GPUBindGroup, FRAMES_IN_FLIGHT> bind_groups{};
        uint                                  frame_slot = 0;
        GPUBindGroupHeap                      heap;
        Vector<GPUBindGroupLayoutHandle>      blayouts;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_EDITOR_GRID_PASS_H
