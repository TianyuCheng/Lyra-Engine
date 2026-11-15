#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H

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
    };

    /**
     * @brief Forward PBR scene rasterization pass.
     * Evaluates standard PBR materials and custom shading models for opaque and transparent scene meshes.
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
         * @brief initializes forward rasterization pipeline, layouts, and uniform buffers.
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

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

    private:
        struct CameraUniforms
        {
            Matrix4x4 proj;
            Matrix4x4 view;
            Matrix4x4 inv_proj;
            Matrix4x4 inv_view;
            Vector3   pos;
            float     padding;
        };

        struct InstanceData
        {
            Matrix4x4 model;
            Vector4   color;
            uint      shading_model;
            Vector3   extra_params;
        };

        void update_camera_buffer(const RenderView& view);

    private:
        GPUDevice                        device;
        GPURenderPipeline                pipeline;
        GPUPipelineLayout                playout;
        GPUShaderModule                  vshader;
        GPUShaderModule                  fshader;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        GPUBindGroupHeap                 heap;
        GPUBuffer                        camera_buffer;
        GPUBindGroup                     camera_bind_group;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H
