#pragma once

#ifndef LYRA_ENGINE_RENDERING_RESOURCE_RETIRE_H
#define LYRA_ENGINE_RENDERING_RESOURCE_RETIRE_H

#include <utility>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>

namespace lyra
{
    /**
     * @brief Deferred resource retirement queue for a specific resource type.
     *
     * Holds resources by value across `frames_in_flight` ticks, invoking T::destroy()
     * once the GPU timeline has safely completed execution.
     *
     * @tparam T Any resource type with a void destroy() method.
     */
    template <typename T>
    struct ResourceRetireQueue
    {
    public:
        ResourceRetireQueue() = default;

        /**
         * @brief Initialize the queue with the number of frames in flight.
         */
        void init(uint frames_in_flight)
        {
            this->frames_in_flight = frames_in_flight;
            this->current_frame    = 0;
            buckets.resize(frames_in_flight);
        }

        /**
         * @brief Enqueue a resource for deferred retirement.
         *        T::destroy() will be called after frames_in_flight ticks.
         *        If the queue is uninitialized, the resource is destroyed immediately.
         */
        void retire(T resource)
        {
            if constexpr (requires { resource.valid(); }) {
                if (!resource.valid()) return;
            }

            if (frames_in_flight == 0 || buckets.empty()) {
                resource.destroy();
                return;
            }

            buckets[current_frame].push_back(std::move(resource));
        }

        /**
         * @brief Advance one frame: flush the oldest bucket then rotate the cursor.
         */
        void tick()
        {
            if (frames_in_flight == 0 || buckets.empty()) return;
            current_frame = (current_frame + 1) % frames_in_flight;

            auto& old_bucket = buckets[current_frame];
            for (auto& resource : old_bucket)
                resource.destroy();
            old_bucket.clear();
        }

        /**
         * @brief Immediately drain all pending retirements regardless of age.
         */
        void drain()
        {
            for (auto& bucket : buckets) {
                for (auto& resource : bucket)
                    resource.destroy();
                bucket.clear();
            }
            current_frame = 0;
        }

        /**
         * @brief Check if all buckets are empty.
         */
        bool empty() const
        {
            for (const auto& bucket : buckets) {
                if (!bucket.empty()) return false;
            }
            return true;
        }

        /**
         * @brief Total count of pending resources across all frame buckets.
         */
        usize size() const
        {
            usize total = 0;
            for (const auto& bucket : buckets)
                total += bucket.size();
            return total;
        }

        /**
         * @brief Number of configured frames in flight.
         */
        uint get_frames_in_flight() const { return frames_in_flight; }

    private:
        uint frames_in_flight = 0;
        uint current_frame    = 0;

        SmallVector<Vector<T>, 4> buckets;
    };

    template <typename T>
    using RetireQueue = ResourceRetireQueue<T>;

    /**
     * @brief Composite deferred GPU resource retirement system.
     *
     * Owns typed ResourceRetireQueue instances for all destroyable GPU resource types,
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
        ResourceRetireQueue<GPUBuffer>             buffers;
        ResourceRetireQueue<GPUTexture>            textures;
        ResourceRetireQueue<GPUTextureView>        texture_views;
        ResourceRetireQueue<GPUSampler>            samplers;
        ResourceRetireQueue<GPUShaderModule>       shaders;
        ResourceRetireQueue<GPUQuerySet>           query_sets;
        ResourceRetireQueue<GPUFence>              fences;
        ResourceRetireQueue<GPUTlas>               tlases;
        ResourceRetireQueue<GPUBlas>               blases;
        ResourceRetireQueue<GPUBindGroupHeap>      bind_group_heaps;
        ResourceRetireQueue<GPUBindGroupLayout>    bind_group_layouts;
        ResourceRetireQueue<GPUPipelineLayout>     pipeline_layouts;
        ResourceRetireQueue<GPURenderPipeline>     render_pipelines;
        ResourceRetireQueue<GPUComputePipeline>    compute_pipelines;
        ResourceRetireQueue<GPURayTracingPipeline> raytracing_pipelines;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RESOURCE_RETIRE_H
