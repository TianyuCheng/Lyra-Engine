#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/MeshPass.h>
#include <Lyra/Rendering/ShadingModelRegistry.h>
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
        FrameGraphHandle<FrameGraphTexture> shadow_map;
        FrameGraphHandle<FrameGraphBuffer>  cluster_grid_buffer;
        FrameGraphHandle<FrameGraphBuffer>  light_index_list;
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
        FrameGraphHandle<FrameGraphTexture> shadow_map;
        FrameGraphHandle<FrameGraphBuffer>  cluster_grid_buffer;
        FrameGraphHandle<FrameGraphBuffer>  light_index_list;
        const GPUBuffer*                    lights_buffer   = nullptr;
        GPUTextureViewHandle                environment_map = {};
        GPUTextureViewHandle                brdf_lut        = {};
        uint                                cluster_grid_x  = 0;
        uint                                cluster_grid_y  = 0;
        uint                                cluster_grid_z  = 0;
        uint                                num_lights      = 0;
        Matrix4x4                           light_view_proj{1.0f};
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
        /**
         * @brief Uniform buffer for view and projection camera matrices.
         * @note Memory layout directly mirrors struct `Camera` in `sys://Shaders/Forward/Forward.slang`.
         *       Any layout modification must be kept strictly in sync across both C++ and Slang definitions.
         */
        struct alignas(16) CameraUniforms
        {
            Matrix4x4 proj;
            Matrix4x4 view;
            Matrix4x4 inv_proj;
            Matrix4x4 inv_view;
            Vector4   pos_and_intensity;
            Vector4   sun_dir_and_pad;
            Vector4   sun_color_and_pad;
            Matrix4x4 light_view_proj = Matrix4x4(1.0f);
            Vector4   cluster_params;
            Vector4   cluster_dims;
            Vector4   env_sh[9];
        };

        static_assert(sizeof(CameraUniforms) == 544, "CameraUniforms must be 544 bytes");
        static_assert(alignof(CameraUniforms) == 16, "CameraUniforms must be 16-byte aligned");

        void update_camera_buffer(
            uint               slot,
            const RenderView&  view,
            const RenderScene& scene,
            const Matrix4x4&   light_vp,
            const GPUExtent2D& extent,
            uint               cluster_grid_x,
            uint               cluster_grid_y,
            uint               cluster_grid_z,
            uint               num_lights);

    private:
        static constexpr uint FRAMES_IN_FLIGHT = 3;

        GPURenderPipeline get_or_create_pipeline(ShadingModelID shading_model_id);

        GPUDevice                                  device;
        Compiler                                   compiler;
        MeshPass                                   mesh_pass;
        HashMap<ShadingModelID, GPURenderPipeline> variant_pipelines;
        Vector<GPUShaderModule>                    variant_shader_modules;
        GPUPipelineLayout                          playout;
        Vector<GPUBindGroupLayoutHandle>           blayouts;
        GPUBindGroupHeap                           heap;
        Array<GPUBuffer, FRAMES_IN_FLIGHT>         camera_buffers{};
        Array<GPUBindGroup, FRAMES_IN_FLIGHT>      camera_bind_groups{};
        GPUBuffer                                  default_cluster_grid_buffer;
        GPUBuffer                                  default_light_index_buffer;
        GPUBuffer                                  default_lights_buffer;
        GPUSampler                                 shadow_sampler;
        GPUTexture                                 default_shadow_map;
        GPUTextureView                             default_shadow_view;
        GPUSampler                                 environment_sampler;
        GPUTexture                                 default_env_map;
        GPUTextureView                             default_env_view;
        GPUTexture                                 default_brdf_lut;
        GPUTextureView                             default_brdf_lut_view;
        uint                                       frame_slot = 0;

        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;

        bool initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_FORWARD_PASS_H
