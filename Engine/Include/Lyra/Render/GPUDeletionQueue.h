#pragma once

#ifndef LYRA_LYRA_RENDER_GPU_DELETION_QUEUE_H
#define LYRA_LYRA_RENDER_GPU_DELETION_QUEUE_H

#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Collections.h>

namespace lyra
{
    /**
     * @brief Deferred GPU resource deletion queue for a specific resource type.
     *
     * Stores GPU resources directly by value (no type erasure, no heap allocation per enqueue)
     * and calls T::destroy() after `frames_in_flight` ticks, ensuring the GPU is no longer
     * consuming the resource.
     *
     * Usage:
     *   1. Call init(frames_in_flight) before first use.
     *   2. Call tick() once per frame at RENDER_PRE.
     *   3. Call enqueue(resource) to schedule a resource for deferred deletion.
     *   4. Call drain() after device idle on shutdown.
     *
     * @tparam T Any GPU resource type with a void destroy() method.
     */
    template <typename T>
    struct GPUDeletionQueue
    {
    public:
        GPUDeletionQueue() = default;

        /**
         * @brief Initialize the queue with the number of frames in flight.
         *        Must be called before tick() or enqueue(). Typically driven by
         *        GPUSurface::get_image_count().
         */
        void init(uint frames_in_flight)
        {
            this->frames_in_flight = frames_in_flight;
            buckets.resize(frames_in_flight);
        }

        /**
         * @brief Enqueue a resource for deferred deletion.
         *        T::destroy() will be called after frames_in_flight ticks.
         */
        void enqueue(T resource)
        {
            buckets[current_frame].push_back(resource);
        }

        /**
         * @brief Advance one frame: flush the oldest bucket then rotate the cursor.
         *        Call once per frame (at RENDER_PRE).
         */
        void tick()
        {
            // advance cursor first so enqueue() during this frame fills the new slot
            current_frame = (current_frame + 1) % frames_in_flight;

            // flush the slot we are about to overwrite — those resources are N frames old
            auto& old_bucket = buckets[current_frame];
            for (auto& resource : old_bucket)
                resource.destroy();
            old_bucket.clear();
        }

        /**
         * @brief Immediately drain all pending deletions regardless of age.
         *        Use on shutdown after device idle.
         */
        void drain()
        {
            for (auto& bucket : buckets) {
                for (auto& resource : bucket)
                    resource.destroy();
                bucket.clear();
            }
        }

    private:
        uint frames_in_flight = 0;
        uint current_frame    = 0;

        Vector<Vector<T>> buckets;
    };

} // namespace lyra

#endif // LYRA_LYRA_RENDER_GPU_DELETION_QUEUE_H
