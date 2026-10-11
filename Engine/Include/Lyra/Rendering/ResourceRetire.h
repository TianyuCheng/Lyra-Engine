#pragma once

#ifndef LYRA_ENGINE_RENDERING_RESOURCE_RETIRE_H
#define LYRA_ENGINE_RENDERING_RESOURCE_RETIRE_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/GPURetireQueue.h>

namespace lyra
{
    /**
     * @brief Composite deferred GPU resource retirement system.
     *
     * Owns typed GPURetireQueue instances for all destroyable GPU resource types,
     * safe-guarding resources until in-flight GPU frames have finished.
     */
    class ResourceRetire
    {
    public:
        explicit ResourceRetire() = default;

        void init(uint frames_in_flight);
        void tick();
        void drain();

        /**
         * @brief Check if all typed retirement queues are empty.
         */
        bool empty() const;

        /**
         * @brief Total count of pending resources across all queues.
         */
        usize size() const;

        /**
         * @brief Enqueue a GPU resource for deferred retirement.
         *        Dispatches to the appropriate typed queue via if constexpr.
         * @tparam T A GPU object type (checked via is_gpu_object).
         */
        template <typename T>
        void retire(T resource)
        {
            static_assert(is_gpu_object_v<T>, "T must be a GPU object type");
            using DecayedT = std::decay_t<T>;

            if constexpr (std::is_same_v<DecayedT, GPUBuffer>)
                buffers.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUTexture>)
                textures.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUTextureView>)
                texture_views.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUSampler>)
                samplers.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUShaderModule>)
                shaders.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUQuerySet>)
                query_sets.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUFence>)
                fences.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUTlas>)
                tlases.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUBlas>)
                blases.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUBindGroupHeap>)
                bind_group_heaps.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUBindGroupLayout>)
                bind_group_layouts.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUPipelineLayout>)
                pipeline_layouts.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPURenderPipeline>)
                render_pipelines.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPUComputePipeline>)
                compute_pipelines.retire(resource);
            else if constexpr (std::is_same_v<DecayedT, GPURayTracingPipeline>)
                raytracing_pipelines.retire(resource);
            else
                static_assert(!sizeof(T), "Unsupported GPU object type for deferred retirement");
        }

        /**
         * @brief Enqueue multiple GPU resources for deferred retirement.
         */
        template <typename T, typename... Rest>
        void retire(T first, Rest... rest)
        {
            static_assert(is_gpu_object_v<T>, "T must be a GPU object type");
            static_assert((is_gpu_object_v<Rest> && ...), "All rest arguments must be GPU object types");
            retire(first);
            (retire(rest), ...);
        }

        /**
         * @brief Access the underlying typed retirement queue for a given GPU object type.
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
                static_assert(!sizeof(T), "Unsupported GPU object type for retirement queue");
        }

    private:
        GPURetireQueue<GPUBuffer>             buffers;
        GPURetireQueue<GPUTexture>            textures;
        GPURetireQueue<GPUTextureView>        texture_views;
        GPURetireQueue<GPUSampler>            samplers;
        GPURetireQueue<GPUShaderModule>       shaders;
        GPURetireQueue<GPUQuerySet>           query_sets;
        GPURetireQueue<GPUFence>              fences;
        GPURetireQueue<GPUTlas>               tlases;
        GPURetireQueue<GPUBlas>               blases;
        GPURetireQueue<GPUBindGroupHeap>      bind_group_heaps;
        GPURetireQueue<GPUBindGroupLayout>    bind_group_layouts;
        GPURetireQueue<GPUPipelineLayout>     pipeline_layouts;
        GPURetireQueue<GPURenderPipeline>     render_pipelines;
        GPURetireQueue<GPUComputePipeline>    compute_pipelines;
        GPURetireQueue<GPURayTracingPipeline> raytracing_pipelines;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RESOURCE_RETIRE_H
