#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/MeshPass.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/GUID.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the forward rasterization pass.
     */
    struct ForwardPassData
    {
        FrameGraphHandle<FrameGraphTexture> color_target;
        FrameGraphHandle<FrameGraphTexture> depth_target;
        uint                                draw_calls         = 0;
        uint                                triangles_rendered = 0;
    };

    /**
     * @brief input parameters for recording the forward pass into the frame graph.
     */
    struct ForwardPassParams
    {
        FrameGraphHandle<FrameGraphTexture> color_target;
        FrameGraphHandle<FrameGraphTexture> depth_target;
        const RenderView&                   view;
        const RenderScene&                  scene;
        GPUExtent2D                         extent               = {0, 0};
        GPUTextureFormat                    color_format         = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat                    depth_format         = GPUTextureFormat::DEPTH32FLOAT;
        bool                                depth_prepass_active = false;
        GPULoadOp                           color_load_op        = GPULoadOp::CLEAR;
        GPULoadOp                           depth_load_op        = GPULoadOp::LOAD;
    };

    /**
     * @brief Forward decoupled scene rasterization pass.
     * Evaluates specialized shading models through MeshPass and MaterialPipelineManager.
     */
    struct ForwardPass
    {
    public:
        explicit ForwardPass() = default;
        ~ForwardPass()         = default;

        ForwardPass(const ForwardPass&)            = delete;
        ForwardPass& operator=(const ForwardPass&) = delete;

        ForwardPass(ForwardPass&&) noexcept            = default;
        ForwardPass& operator=(ForwardPass&&) noexcept = default;

        /**
         * @brief initializes forward rasterization pipeline layouts and uniform buffers.
         */
        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat color_format = GPUTextureFormat::RGBA8UNORM,
            GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT,
            const Path&      shaders_root = "sys://Shaders");

        /**
         * @brief safely destroys GPU pipeline, layouts, and buffers.
         */
        void destroy();

        /**
         * @brief records forward rasterization into the frame graph.
         */
        auto add(
            FrameGraphBuilder&       builder,
            const ForwardPassParams& params) -> ForwardPassData;

        [[nodiscard]] bool valid() const { return initialized; }

    private:
        struct CameraUniforms
        {
            Matrix4x4 proj;
            Matrix4x4 view;
            Matrix4x4 inv_proj;
            Matrix4x4 inv_view;
            Vector3   pos;
            float     sun_intensity = 2.5f;
            Vector3   sun_dir       = Vector3(0.5f, 1.0f, 0.3f);
            float     padding       = 0.0f;
            Vector3   sun_color     = Vector3(1.0f);
            float     padding2      = 0.0f;
        };

        void update_camera_buffer(uint slot, const RenderView& view, const RenderScene& scene);

    private:
        static constexpr uint FRAMES_IN_FLIGHT = 3;

        GPURenderPipeline get_or_create_pipeline(GUID shading_model_id);

        GPUDevice                             device;
        Compiler                              compiler;
        MeshPass                              mesh_pass;
        HashMap<GUID, GPURenderPipeline>      variant_pipelines;
        Vector<GPUShaderModule>               variant_shader_modules;
        GPUPipelineLayout                     playout;
        Vector<GPUBindGroupLayoutHandle>      blayouts;
        GPUBindGroupHeap                      heap;
        Array<GPUBuffer, FRAMES_IN_FLIGHT>    camera_buffers{};
        Array<GPUBindGroup, FRAMES_IN_FLIGHT> camera_bind_groups{};
        uint                                  frame_slot = 0;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;

        bool initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H
