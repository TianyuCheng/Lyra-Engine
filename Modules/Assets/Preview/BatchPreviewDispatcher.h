#pragma once

#ifndef LYRA_MODULES_ASSETS_PREVIEW_BATCHPREVIEWDISPATCHER_H
#define LYRA_MODULES_ASSETS_PREVIEW_BATCHPREVIEWDISPATCHER_H

#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Promise.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Sync.h>
#include <Lyra/Assets/AMSPreview.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>

namespace lyra::preview
{
    /**
     * @brief Pipeline-wide preview rasterizer configuration.
     */
    struct PipelinePreviewConfig
    {
        Path    caches_root;
        uint    width       = 128;
        uint    height      = 128;
        float   pitch_deg   = 25.0f;
        float   yaw_deg     = 45.0f;
        float   fov_deg     = 35.0f;
        Vector4 clear_color = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        bool    enable_ssaa = true;
    };

    /**
     * @brief Retrieve global preview configuration instance.
     */
    auto get_config() -> PipelinePreviewConfig&;

    /**
     * @brief Asynchronous coalescing GPU preview dispatcher.
     * Batches multiple 3D preview render jobs into a single atlas render pass and single command buffer.
     */
    class BatchPreviewDispatcher
    {
    public:
        static auto instance() -> BatchPreviewDispatcher&;

        void configure(const AssetConfigContext& context);
        auto generate_thumbnail(const PreviewScene& scene, JSON& metadata) -> Future<Path>;
        auto render_scene(const PreviewScene& scene) -> PreviewTexture;
        void flush();
        void shutdown();

    private:
        static constexpr usize MAX_BATCH_SIZE = 64;

        struct PreviewRequest
        {
            AssetID            guid = 0;
            PreviewScene       scene;
            Path               thumb_path;
            Path               rel_path;
            Ref<Promise<Path>> promise;
        };

        struct PreviewVertex
        {
            Vector3 position;
            Vector3 normal;
            Vector2 uv;
        };

        struct alignas(16) PreviewDrawConstants
        {
            Matrix4x4 vp;
            Vector4   base_color;
            Vector4   view_dir_pad;
        };

        struct DrawBatchItem
        {
            uint         index_count  = 0;
            uint         first_index  = 0;
            int          base_vertex  = 0;
            Matrix4x4    vp           = Matrix4x4(1.0f);
            Vector4      base_color   = Vector4(1.0f);
            Vector4      view_dir_pad = Vector4(0.0f);
            GPUBindGroup bind_group;
            uint         tile_x = 0;
            uint         tile_y = 0;
            uint         tile_w = 0;
            uint         tile_h = 0;
        };

        mutable Mutex          mutex;
        Vector<PreviewRequest> pending_queue;
        bool                   timer_armed     = false;
        bool                   gpu_initialized = false;
        bool                   gpu_failed      = false;

        // gpu resources
        CompilerHandle          external_compiler;
        OwnedResource<Compiler> owned_compiler;
        Compiler                compiler;
        GPUBindGroupLayout      bind_group_layout;
        GPUBindGroupHeap        bind_group_heap;
        GPUPipelineLayout       pipeline_layout;
        GPURenderPipeline       pipeline;
        GPUSampler              default_sampler;
        GPUTexture              default_white_texture;
        GPUTextureView          default_white_view;
        GPUBindGroup            default_white_bind_group;

        GPUTexture     atlas_color;
        GPUTextureView atlas_color_view;
        GPUTexture     atlas_depth;
        GPUTextureView atlas_depth_view;
        GPUBuffer      readback_buffer;

        GPUBuffer vbo;
        GPUBuffer ibo;
        GPUBuffer vbo_staging;
        GPUBuffer ibo_staging;
        usize     vbo_capacity = 0;
        usize     ibo_capacity = 0;

        uint atlas_w = 2048;
        uint atlas_h = 2048;

        auto init_gpu_resources() -> bool;
        void destroy_gpu_resources();
        void dispatch_batch_locked();
        void arm_debounce_timer_locked();
    };

} // namespace lyra::preview

#endif // LYRA_MODULES_ASSETS_PREVIEW_BATCHPREVIEWDISPATCHER_H
