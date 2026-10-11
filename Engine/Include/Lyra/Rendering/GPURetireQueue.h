#pragma once

#ifndef LYRA_ENGINE_RENDERING_GPU_RETIRE_QUEUE_H
#define LYRA_ENGINE_RENDERING_GPU_RETIRE_QUEUE_H

#include <utility>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>

namespace lyra
{
    /**
     * @brief Deferred GPU resource retirement queue for a specific resource type.
     *
     * Holds GPU resources by value across `frames_in_flight` ticks, invoking T::destroy()
     * once the GPU timeline has safely completed execution.
     *
     * @tparam T Any GPU resource type with a void destroy() method.
     */
    template <typename T>
    struct GPURetireQueue
    {
    public:
        GPURetireQueue() = default;

        /**
         * @brief Initialize the queue with the number of frames in flight.
         *        Must be called before tick() or retire(). Typically driven by
         *        GPUSurface::get_image_count().
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
         *        Call once per frame (at RENDER_PRE).
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
         *        Use on shutdown after device idle.
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

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_GPU_RETIRE_QUEUE_H
