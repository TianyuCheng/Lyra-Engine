#pragma once

#ifndef LYRA_LYRA_ENGINE_RENDER_LAYER_H
#define LYRA_LYRA_ENGINE_RENDER_LAYER_H

#include <Lyra/Scenes/World.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/Render/GPUDeletionQueue.h>

// local import
#include <Lyra/Engine/Application.h>

namespace lyra
{
    /**
     * @brief The RenderLayer manages GPU rendering infrastructure and camera ECS systems.
     *
     * Responsibilities:
     *  - Owns typed GPUDeletionQueues for all destroyable GPU resource types (private).
     *  - Exposes convenience template methods (enqueue / destroy_later) to defer GPU resource destruction.
     *  - Ticks all deletion queues at RENDER_PRE (safe deferred GPU resource destruction).
     *  - Flushes all pending deletions at DESTROY after device idle.
     *  - Drives perspective and orthographic camera projection updates at UPDATE_PRE.
     *  - Registers RenderLayer* on the Blackboard for render passes to enqueue deletions.
     *
     * Resources can be enqueued for deferred deletion via:
     *   blackboard.get<RenderLayer*>()->enqueue(my_buffer);
     *   blackboard.get<RenderLayer*>()->enqueue(my_texture, my_view);
     */
    struct RenderLayer
    {
    public:
        /**
         * @brief Default constructor. Deletion queues are sized lazily in bind().
         */
        explicit RenderLayer();

        /**
         * @brief Register this layer on the blackboard and bind lifecycle events.
         *        Sizes all deletion queues from GPUSurface::get_image_count().
         */
        void bind(Application& app);

        /**
         * @brief Called at UPDATE_PRE: update camera projection matrices for all cameras.
         */
        void update(Blackboard&);

        /**
         * @brief Called at RENDER_PRE: advance all deletion queues by one frame.
         */
        void pre_render(Blackboard&);

        /**
         * @brief Called at DESTROY: device idle, then drain all pending deletions.
         */
        void destroy(Blackboard&);

        /**
         * @brief Immediately drain all pending deletions across all queues.
         */
        void drain();

        /**
         * @brief Enqueue a GPU resource for deferred deletion.
         *        Dispatches to the appropriate typed queue via if constexpr.
         * @tparam T A GPU object type (checked via is_gpu_object).
         */
        template <typename T>
        void enqueue(T resource)
        {
            static_assert(is_gpu_object_v<T>, "T must be a GPU object type");
            using DecayedT = std::decay_t<T>;

            if constexpr (std::is_same_v<DecayedT, GPUBuffer>)
                buffers.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUTexture>)
                textures.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUTextureView>)
                texture_views.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUSampler>)
                samplers.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUShaderModule>)
                shaders.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUQuerySet>)
                query_sets.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUFence>)
                fences.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUTlas>)
                tlases.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUBlas>)
                blases.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUBindGroupHeap>)
                bind_group_heaps.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUBindGroupLayout>)
                bind_group_layouts.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUPipelineLayout>)
                pipeline_layouts.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPURenderPipeline>)
                render_pipelines.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUComputePipeline>)
                compute_pipelines.enqueue(resource);
            else if constexpr (std::is_same_v<DecayedT, GPURayTracingPipeline>)
                raytracing_pipelines.enqueue(resource);
            else
                static_assert(!sizeof(T), "Unsupported GPU object type for deferred deletion");
        }

        /**
         * @brief Enqueue multiple GPU resources for deferred deletion.
         */
        template <typename T, typename... Rest>
        void enqueue(T first, Rest... rest)
        {
            static_assert(is_gpu_object_v<T>, "T must be a GPU object type");
            static_assert((is_gpu_object_v<Rest> && ...), "All rest arguments must be GPU object types");
            enqueue(first);
            (enqueue(rest), ...);
        }

        /**
         * @brief Convenience alias for enqueue().
         */
        template <typename... Args>
        void destroy_later(Args&&... args)
        {
            enqueue(std::forward<Args>(args)...);
        }

        /**
         * @brief Access the underlying typed deletion queue for a given GPU object type.
         */
        template <typename T>
        auto& get_queue()
        {
            static_assert(is_gpu_object_v<T>, "T must be a GPU object type");
            using DecayedT = std::decay_t<T>;

            if constexpr (std::is_same_v<DecayedT, GPUBuffer>)
                return buffers;
            else if constexpr (std::is_same_v<DecayedT, GPUTexture>)
                return textures;
            else if constexpr (std::is_same_v<DecayedT, GPUTextureView>)
                return texture_views;
            else if constexpr (std::is_same_v<DecayedT, GPUSampler>)
                return samplers;
            else if constexpr (std::is_same_v<DecayedT, GPUShaderModule>)
                return shaders;
            else if constexpr (std::is_same_v<DecayedT, GPUQuerySet>)
                return query_sets;
            else if constexpr (std::is_same_v<DecayedT, GPUFence>)
                return fences;
            else if constexpr (std::is_same_v<DecayedT, GPUTlas>)
                return tlases;
            else if constexpr (std::is_same_v<DecayedT, GPUBlas>)
                return blases;
            else if constexpr (std::is_same_v<DecayedT, GPUBindGroupHeap>)
                return bind_group_heaps;
            else if constexpr (std::is_same_v<DecayedT, GPUBindGroupLayout>)
                return bind_group_layouts;
            else if constexpr (std::is_same_v<DecayedT, GPUPipelineLayout>)
                return pipeline_layouts;
            else if constexpr (std::is_same_v<DecayedT, GPURenderPipeline>)
                return render_pipelines;
            else if constexpr (std::is_same_v<DecayedT, GPUComputePipeline>)
                return compute_pipelines;
            else if constexpr (std::is_same_v<DecayedT, GPURayTracingPipeline>)
                return raytracing_pipelines;
            else
                static_assert(!sizeof(T), "Unsupported GPU object type for deletion queue");
        }

    private:
        /**
         * @brief Recalculates projection matrices for all perspective cameras.
         */
        void update_perspective(World& world);

        /**
         * @brief Recalculates projection matrices for all orthographic cameras.
         */
        void update_orthographic(World& world);

    private:
        // -- per-type deferred deletion queues --
        GPUDeletionQueue<GPUBuffer>             buffers;
        GPUDeletionQueue<GPUTexture>            textures;
        GPUDeletionQueue<GPUTextureView>        texture_views;
        GPUDeletionQueue<GPUSampler>            samplers;
        GPUDeletionQueue<GPUShaderModule>       shaders;
        GPUDeletionQueue<GPUQuerySet>           query_sets;
        GPUDeletionQueue<GPUFence>              fences;
        GPUDeletionQueue<GPUTlas>               tlases;
        GPUDeletionQueue<GPUBlas>               blases;
        GPUDeletionQueue<GPUBindGroupHeap>      bind_group_heaps;
        GPUDeletionQueue<GPUBindGroupLayout>    bind_group_layouts;
        GPUDeletionQueue<GPUPipelineLayout>     pipeline_layouts;
        GPUDeletionQueue<GPURenderPipeline>     render_pipelines;
        GPUDeletionQueue<GPUComputePipeline>    compute_pipelines;
        GPUDeletionQueue<GPURayTracingPipeline> raytracing_pipelines;
    };

} // namespace lyra

#endif // LYRA_LYRA_ENGINE_RENDER_LAYER_H
